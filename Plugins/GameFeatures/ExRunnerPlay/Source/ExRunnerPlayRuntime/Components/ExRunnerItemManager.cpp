// Copyright ExFrameWork. All Rights Reserved.

#include "ExRunnerItemManager.h"
#include "ExItemSystemTypes.h"
#include "ExItemDefinition.h"
#include "ExItemPickupBase.h"
#include "ExRunnerItemSpawnTable.h"
#include "ExObstacleManager.h"
#include "ExFloorChunk.h"
#include "ExChunkSpawner.h"
#include "FExSpawnPlan.h"
#include "Curves/CurveFloat.h"
#include "Components/SphereComponent.h"
#include "Subsystems/ExDataCenterSubsystem.h"
#include "ExGameplayEventSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

UExRunnerItemManager::UExRunnerItemManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UExRunnerItemManager::BeginPlay()
{
	Super::BeginPlay();

	// ── SpawnTable 조기 로드 제거 ──
	// DataCenter는 GameFeature(ExRunnerPlay) 활성화 이후에만 사용 가능합니다.
	// BeginPlay 시점에는 Experience 로드가 완료되지 않아 DataCenter가 비어 있으므로
	// GetPreset 호출이 실패하고 화면에 오류 메시지가 출력됩니다.
	// SpawnTable은 EnsureSpawnTableLoaded()의 재시도 로직에 의해 최초 사용 시점에
	// 자동으로 지연 로드됩니다 (SpawnItemsOnChunk, GenerateItemPlan 등).

	// 클라이언트 측 수동 Attach 이벤트 수신 대기 (AExItemPickupBase에서 발송)
	if (UWorld* World = GetWorld())
	{
		if (UExGameplayEventSubsystem* EventSub = World->GetSubsystem<UExGameplayEventSubsystem>())
		{
			FGameplayTag AttachTag = FGameplayTag::RequestGameplayTag(FName("Event.Sync.ItemReAttach"), false);
			if (AttachTag.IsValid())
			{
				EventSub->GetEventDelegate(AttachTag).AddDynamic(this, &UExRunnerItemManager::HandleItemReAttachEvent);
			}
		}
	}
}

void UExRunnerItemManager::EnsureSpawnTableLoaded()
{
	if (!CachedSpawnTable && SpawnTableTag.IsValid())
	{
		if (UGameInstance* GI = GetWorld()->GetGameInstance())
		{
			if (UExDataCenterSubsystem* DataCenter = GI->GetSubsystem<UExDataCenterSubsystem>())
			{
				CachedSpawnTable = DataCenter->GetPreset<UExRunnerItemSpawnTable>(SpawnTableTag);
			}
		}
	}
}

// ── Z축 배치 결정 ──

float UExRunnerItemManager::CalculateItemZ(const FExObstacleContext& Context, float ChunkBaseZ, float AlphaInGap) const
{
	if (!Context.bHasObstacle)
	{
		// 장애물 없음 → 기본 배치 높이 (+ 파묻힘 방지 오프셋)
		return ChunkBaseZ + ItemBaseZOffset;
	}

	switch (Context.ObstacleType)
	{
	case EExObstacleType::Climb:
		// 장애물 꼭대기 위에 배치 (+ 여유 오프셋)
		return Context.ObstacleTopZ + ItemTopPlacementOffset + ItemBaseZOffset;

	case EExObstacleType::Slide:
		// §3.4: FMath::FRand() → ItemRandomStream.FRand()로 교체 (결정론 보장)
		// 주의: CalculateItemZ는 GenerateItemPlan 내에서만 호출해야 스트림 소비 순서가 보장됩니다.
		if (Context.bCanClimbOver && const_cast<UExRunnerItemManager*>(this)->ItemRandomStream.FRand() < SlideTopPlacementRatio)
		{
			// 올라갈 수 있는 Slide: 확률적으로 꼭대기 배치
			return Context.ObstacleTopZ + ItemTopPlacementOffset + ItemBaseZOffset;
		}
		// 바닥과 장애물 하단 사이 빈 공간 중앙
		return (ChunkBaseZ + Context.ObstacleBottomZ) * 0.5f;

	case EExObstacleType::Gap:
		// 점프 포물선 커브 평가
		if (JumpArcCurve)
		{
			return ChunkBaseZ + (JumpArcCurve->GetFloatValue(AlphaInGap) * JumpApexHeight) + ItemBaseZOffset;
		}
		// 커브 미할당 시 기본 사인(Sin) 포물선 궤적 적용
		return ChunkBaseZ + (FMath::Sin(AlphaInGap * UE_PI) * JumpApexHeight) + ItemBaseZOffset;

	case EExObstacleType::WallRun:
		// 벽 달리기 중간 높이
		return ChunkBaseZ + (Context.ObstacleTopZ - ChunkBaseZ) * 0.5f;

	default:
		return ChunkBaseZ + ItemBaseZOffset;
	}
}

// ── 생명주기 관리 및 연결 ──

void UExRunnerItemManager::BindToSpawner(UExChunkSpawner* Spawner)
{
	if (Spawner)
	{
		Spawner->OnChunkDespawned.AddDynamic(this, &UExRunnerItemManager::OnChunkDespawned);
		UE_LOG(LogExItemSystem, Log, TEXT("[ExRunnerItemManager] Bound to ChunkSpawner: %s"), *Spawner->GetName());
	}
}

void UExRunnerItemManager::OnChunkDespawned(AExFloorChunk* Chunk)
{
	if (!Chunk) return;

	// Phase 4: SpawnedBySegment 인덱스 기반 회수
	if (const TArray<TWeakObjectPtr<AExItemPickupBase>>* SegItems = SpawnedBySegment.Find(Chunk->SegmentIndex))
	{
		for (const TWeakObjectPtr<AExItemPickupBase>& WeakItem : *SegItems)
		{
			if (AExItemPickupBase* Item = WeakItem.Get())
			{
				ReturnItemToPool(Item);
			}
		}
		SpawnedBySegment.Remove(Chunk->SegmentIndex);
	}
	else
	{
		// 레거시 폴백: Attach 기반 회수
		TArray<AActor*> AttachedActors;
		Chunk->GetAttachedActors(AttachedActors);
		for (AActor* Attached : AttachedActors)
		{
			if (AExItemPickupBase* Item = Cast<AExItemPickupBase>(Attached))
			{
				ReturnItemToPool(Item);
			}
		}
	}
}

// ── Phase 1: 결정론 RandomStream 초기화 ──

void UExRunnerItemManager::InitializeRandomStream(int32 SharedSeed)
{
	// Hash(SharedSeed, 2)로 Item 전용 스트림 파생
	const int32 ItemSeed = HashCombine(SharedSeed, 2);
	ItemRandomStream.Initialize(ItemSeed);
	bRandomStreamInitialized = true;

	// 뱀 패턴 상태값 리셋 (시드 변경 시 반드시 초기화)
	PersistentNextCoinDistance = 0.f;
	CurrentLaneYOffset = 0.f;
	RemainingCoinsInCurrentLane = 0;
	PersistentTargetLane = 0;
	LastItemSafeEndDistance = -99999.f;

	UE_LOG(LogExItemSystem, Log, TEXT("[ExRunnerItemManager] RandomStream 초기화 완료 (SharedSeed=%d, ItemSeed=%d)"),
		SharedSeed, ItemSeed);
}

// ── Phase 3: GenerateItemPlan ──

TArray<FExSpawnPlan> UExRunnerItemManager::GenerateItemPlan(AExFloorChunk* TargetChunk, const TArray<FExSpawnPlan>& ObstaclePlan)
{
	// 초기화 계약 위반 감지
	ensureMsgf(bRandomStreamInitialized,
		TEXT("[ExRunnerItemManager] GenerateItemPlan 호출 시 RandomStream이 미초기화. InitializeRandomStream을 먼저 호출하세요."));

	TArray<FExSpawnPlan> ResultPlan;

	if (!TargetChunk) return ResultPlan;

	EnsureSpawnTableLoaded();
	if (!CachedSpawnTable) return ResultPlan;

	const float ChunkLength = TargetChunk->ChunkLength;
	const float SafeStart = PersistentNextCoinDistance;
	const float SafeEnd = ChunkLength;

	if (SafeStart >= SafeEnd)
	{
		PersistentNextCoinDistance -= ChunkLength;
		return ResultPlan;
	}

	// 코인 라인 스폰 확률 판정 (결정론 스트림)
	bool bCoinLineSpawned = false;
	if (ItemRandomStream.FRand() < CachedSpawnTable->CoinLineSpawnProbability)
	{
		GenerateCoinLinePlan(TargetChunk, ObstaclePlan, SafeStart, SafeEnd, ResultPlan);
		bCoinLineSpawned = true;
	}
	else
	{
		PersistentNextCoinDistance = 0.f;
		CurrentLaneYOffset = 0.f;
		RemainingCoinsInCurrentLane = 0;
	}

	// 단독 버프 배치 (결정론 스트림)
	if (!bCoinLineSpawned && ItemRandomStream.FRand() < CachedSpawnTable->BuffSoloSpawnProbability)
	{
		const float BuffDist = ItemRandomStream.FRandRange(SafeStart, SafeEnd);
		const float SnakeOffset = CachedSpawnTable->bUseSnakePattern ? CurrentLaneYOffset : 0.f;
		GenerateBuffItemPlan(TargetChunk, ObstaclePlan, BuffDist, SnakeOffset, ResultPlan);
	}

	return ResultPlan;
}

void UExRunnerItemManager::RealizeItemPlan(const TArray<FExSpawnPlan>& Plan, AExFloorChunk* TargetChunk)
{
	// §3.4 서버 권한 이중 가드: 차단 + 알림
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		ensureMsgf(false, TEXT("[ExRunnerItemManager] RealizeItemPlan이 클라이언트에서 호출됨. 서버 전용 함수입니다."));
		return;
	}

	if (!TargetChunk) return;

	for (const FExSpawnPlan& SpawnPlan : Plan)
	{
		if (!SpawnPlan.ActorClass) continue;

		// ItemDefinition 역참조
		EnsureSpawnTableLoaded();
		const UExItemDefinition* ItemDef = nullptr;
		if (CachedSpawnTable)
		{
			// ActorClass로 매칭되는 Definition 탐색
			for (const FExItemSpawnEntry& Entry : CachedSpawnTable->CoinEntries)
			{
				if (Entry.ItemDefinition && Entry.ItemDefinition->PickupActorClass == SpawnPlan.ActorClass)
				{
					ItemDef = Entry.ItemDefinition;
					break;
				}
			}
			if (!ItemDef)
			{
				for (const FExItemSpawnEntry& Entry : CachedSpawnTable->BuffEntries)
				{
					if (Entry.ItemDefinition && Entry.ItemDefinition->PickupActorClass == SpawnPlan.ActorClass)
					{
						ItemDef = Entry.ItemDefinition;
						break;
					}
				}
			}
		}

		if (!ItemDef) continue;

		// Phase 4: World Space 스폰 (Attach 호출하지 않음)
		AExItemPickupBase* SpawnedItem = SpawnItem(ItemDef,
			FTransform(SpawnPlan.WorldRotation, FVector(SpawnPlan.WorldLocation.X, SpawnPlan.WorldLocation.Y, SpawnPlan.PlacedZ)));

		if (SpawnedItem)
		{
			SpawnedItem->OwnerSegmentIndex = SpawnPlan.OwnerSegmentIndex;

			// 서버가 계산한 최종 월드 Transform을 복제 필드에 저장.
			// 클라이언트가 Attach 시점에 이 값을 강제 적용하여 커브 구간 위치/회전 오차를 제거한다.
			SpawnedItem->ReplicatedServerWorldLocation = SpawnPlan.WorldLocation;
			SpawnedItem->ReplicatedServerWorldRotation = SpawnPlan.WorldRotation;

			// SpawnedBySegment 인덱스 등록
			TArray<TWeakObjectPtr<AExItemPickupBase>>& SegArray = SpawnedBySegment.FindOrAdd(SpawnPlan.OwnerSegmentIndex);
			SegArray.Add(SpawnedItem);
		}
	}
}

void UExRunnerItemManager::OnLocalChunkSpawned(AExFloorChunk* SpawnedChunk)
{
	if (!SpawnedChunk) return;

	for (int32 i = PendingAttachQueue.Num() - 1; i >= 0; --i)
	{
		AExItemPickupBase* PendingItem = PendingAttachQueue[i].Get();
		if (!PendingItem)
		{
			PendingAttachQueue.RemoveAt(i);
			continue;
		}

		if (PendingItem->OwnerSegmentIndex == SpawnedChunk->SegmentIndex)
		{
			PendingItem->AttachToActor(SpawnedChunk, FAttachmentTransformRules::KeepWorldTransform);
			PendingItem->SetActorHiddenInGame(false); // 가시성 강제 활성화
			
			TArray<TWeakObjectPtr<AExItemPickupBase>>& SegArray = SpawnedBySegment.FindOrAdd(SpawnedChunk->SegmentIndex);
			SegArray.AddUnique(PendingItem);
			
			PendingAttachQueue.RemoveAt(i);
		}
	}
}

void UExRunnerItemManager::RequestManualAttach(AExItemPickupBase* Item, int32 SegmentIndex)
{
	if (!Item || (GetOwner() && GetOwner()->HasAuthority())) return;

	AExFloorChunk* TargetChunk = nullptr;
	if (UExChunkSpawner* Spawner = GetOwner() ? GetOwner()->FindComponentByClass<UExChunkSpawner>() : nullptr)
	{
		for (AExFloorChunk* Chunk : Spawner->GetActiveChunks())
		{
			if (Chunk && Chunk->SegmentIndex == SegmentIndex)
			{
				TargetChunk = Chunk;
				break;
			}
		}
	}

	if (TargetChunk)
	{
		Item->AttachToActor(TargetChunk, FAttachmentTransformRules::KeepWorldTransform);

		// 서버가 RealizeItemPlan에서 계산한 최종 World Transform을 강제 적용.
		// ReplicatedMovement 기반 위치는 커브 청크 이후 방향이 바뀐 구간에서 부정확할 수 있으므로,
		// 서버 권위 데이터를 명시적으로 덮어써 클라이언트 시각 정합성을 보장한다.
		if (!Item->ReplicatedServerWorldLocation.IsZero() || !Item->ReplicatedServerWorldRotation.IsZero())
		{
			Item->SetActorLocationAndRotation(
				Item->ReplicatedServerWorldLocation,
				Item->ReplicatedServerWorldRotation,
				false,
				nullptr,
				ETeleportType::TeleportPhysics
			);
		}

		Item->SetActorHiddenInGame(false); // 가시성 강제 활성화
		TArray<TWeakObjectPtr<AExItemPickupBase>>& SegArray = SpawnedBySegment.FindOrAdd(SegmentIndex);
		SegArray.AddUnique(Item);
	}
	else
	{
		PendingAttachQueue.AddUnique(Item);
	}
}

void UExRunnerItemManager::HandleItemReAttachEvent(FGameplayTag EventTag, const FExGameplayEventPayload& Payload)
{
	if (AExItemPickupBase* Item = Cast<AExItemPickupBase>(Payload.Instigator))
	{
		int32 SegIdx = FMath::RoundToInt(Payload.OptionalValue);
		RequestManualAttach(Item, SegIdx);
	}
}

// ── Phase 3 헬퍼: Plan 기반 코인 라인 산출 ──

void UExRunnerItemManager::GenerateCoinLinePlan(AExFloorChunk* Chunk, const TArray<FExSpawnPlan>& ObstaclePlan,
	float StartDistance, float EndDistance, TArray<FExSpawnPlan>& OutPlan)
{
	if (!Chunk || !CachedSpawnTable) return;

	// 코인 Entry 조회
	TSubclassOf<AActor> CoinClass = nullptr;
	for (const FExItemSpawnEntry& Entry : CachedSpawnTable->CoinEntries)
	{
		if (Entry.ItemDefinition && Entry.ItemDefinition->PickupActorClass)
		{
			CoinClass = Entry.ItemDefinition->PickupActorClass;
			break;
		}
	}
	if (!CoinClass) return;

	const float Spacing = FMath::Max(CachedSpawnTable->CoinSpacing, 50.f);
	float CurrentDistance = StartDistance;
	const int32 MaxIter = 100;

	// 바닥의 실제 Y 범위 구해 LaneWidth 산출 (레벨 디자인상 3등분 폭)
	float LaneWidth = 100.f;
	if (Chunk)
	{
		float TotalWidth = Chunk->GetFloorBounds().GetSize().Y * Chunk->GetActorScale3D().Y;
		LaneWidth = TotalWidth / 3.0f;
	}

	// 버프 교체 인덱스 결정 (결정론 스트림)
	int32 EstimatedSpawns = FMath::Max(1, FMath::CeilToInt((EndDistance - StartDistance) / Spacing));
	int32 BuffReplaceIndex = -1;
	if (ItemRandomStream.FRand() < CachedSpawnTable->BuffSpawnProbability)
	{
		BuffReplaceIndex = ItemRandomStream.RandRange(0, EstimatedSpawns - 1);
	}

	TSubclassOf<AActor> BuffClass = nullptr;
	if (BuffReplaceIndex >= 0)
	{
		for (const FExItemSpawnEntry& Entry : CachedSpawnTable->BuffEntries)
		{
			if (Entry.ItemDefinition && Entry.ItemDefinition->PickupActorClass)
			{
				BuffClass = Entry.ItemDefinition->PickupActorClass;
				break;
			}
		}
	}

	for (int32 i = 0; i < MaxIter && CurrentDistance < EndDistance; ++i)
	{
		// 레인 결정 (결정론 스트림)
		if (RemainingCoinsInCurrentLane <= 0)
		{
			TArray<int32> PossibleLanes = {-1, 0, 1};
			PossibleLanes.Remove(PersistentTargetLane);
			PersistentTargetLane = PossibleLanes[ItemRandomStream.RandRange(0, PossibleLanes.Num() - 1)];
			RemainingCoinsInCurrentLane = ItemRandomStream.RandRange(
				CachedSpawnTable->MinCoinsPerLine, CachedSpawnTable->MaxCoinsPerLine);
		}

		// 끊김 판정 (결정론 스트림)
		if (i > 0 && ItemRandomStream.FRand() < CachedSpawnTable->CoinLineBreakProbability)
		{
			CurrentDistance += Spacing * 2.f;
			RemainingCoinsInCurrentLane = 0;
			
			// 끊김이 발생하면 대각선 뱀 패턴 스킵 후, 곧장 새 목표 레인으로 이동(스냅)
			CurrentLaneYOffset = PersistentTargetLane * LaneWidth;
			continue;
		}

		// 뱀(Snake) 패턴 보간 계산 (목표 레인을 향해 한 걸음씩 드리프트)
		if (CachedSpawnTable->bUseSnakePattern)
		{
			float TargetY = PersistentTargetLane * LaneWidth;
			float DistanceToTarget = TargetY - CurrentLaneYOffset;
			
			if (FMath::Abs(DistanceToTarget) > KINDA_SMALL_NUMBER)
			{
				float Drift = CachedSpawnTable->LateralDriftPerCoin;
				if (FMath::Abs(DistanceToTarget) <= Drift)
				{
					CurrentLaneYOffset = TargetY;
				}
				else
				{
					CurrentLaneYOffset += FMath::Sign(DistanceToTarget) * Drift;
				}
			}
		}
		else
		{
			CurrentLaneYOffset = PersistentTargetLane * LaneWidth;
		}

		// §3.4 Plan 기반 장애물 질의
		FExObstacleContext ObstCtx;
		if (ObstaclePlan.Num() > 0)
		{
			// Plan.WorldLocation.X로 PathDistance 근사
			// ChunkStartDist + LocalPathOffset = 월드 X
			const float QueryDist = Chunk->GetActorLocation().X + CurrentDistance;
			const float QueryRadius = Spacing * 0.5f;
			for (const FExSpawnPlan& Op : ObstaclePlan)
			{
				if (FMath::Abs(Op.WorldLocation.X - QueryDist) < QueryRadius)
				{
					ObstCtx.bHasObstacle = true;
					ObstCtx.ObstacleType = (EExObstacleType)Op.ObstacleTypeRaw;
					ObstCtx.ObstacleTopZ = Op.WorldLocation.Z + 100.f;
					ObstCtx.ObstacleBottomZ = Op.WorldLocation.Z;
					ObstCtx.bCanClimbOver = (ObstCtx.ObstacleType == EExObstacleType::Slide);
					break;
				}
			}
		}

		// 로컬 트랜스폼 계산 (커브 청크 진행 방향 반영)
		FTransform LocalTrans = Chunk->GetLocalTransformAtDistance(CurrentDistance);

		// 레인 Y 오프셋을 스플라인 RightVector 기준으로 적용
		// 월드 Y 직접 가산이 아닌 로컬 Right 방향으로 이동 → 커브 이후에도 바닥 안쪽에 위치
		if (!FMath::IsNearlyZero(CurrentLaneYOffset))
		{
			FVector SplineRight = LocalTrans.GetRotation().GetRightVector();
			LocalTrans.SetLocation(LocalTrans.GetLocation() + SplineRight * CurrentLaneYOffset);
		}

		// LocalTransform * 청크 WorldTransform → 청크 회전 포함된 완전한 월드 Transform 산출
		FTransform GlobalTrans = LocalTrans * Chunk->GetActorTransform();

		// Z 계산: GlobalTrans의 Z를 기준으로 사용 (해당 LocalDistance의 실제 바닥 Z)
		const float ActualFloorZ = GlobalTrans.GetLocation().Z;
		const float PlacedZ = CalculateItemZ(ObstCtx, ActualFloorZ, 0.5f);

		FVector WorldPos = GlobalTrans.GetLocation();
		WorldPos.Z = PlacedZ;

		// Plan 생성
		FExSpawnPlan Plan;
		Plan.OwnerSegmentIndex = Chunk->SegmentIndex;
		Plan.LocalPathOffset = CurrentDistance;
		Plan.ActorClass = (i == BuffReplaceIndex && BuffClass) ? BuffClass : CoinClass;
		Plan.PlacedZ = PlacedZ;
		Plan.WorldLocation = WorldPos;
		Plan.WorldRotation = GlobalTrans.Rotator(); // 청크 회전 포함된 월드 회전

		OutPlan.Add(Plan);

		RemainingCoinsInCurrentLane--;
		CurrentDistance += Spacing;
	}

	// 이월값 업데이트
	PersistentNextCoinDistance = (CurrentDistance > EndDistance) ? CurrentDistance - EndDistance : 0.f;
}

void UExRunnerItemManager::GenerateBuffItemPlan(AExFloorChunk* Chunk, const TArray<FExSpawnPlan>& ObstaclePlan,
	float AtDistance, float LateralOffset, TArray<FExSpawnPlan>& OutPlan)
{
	if (!Chunk || !CachedSpawnTable) return;

	TSubclassOf<AActor> BuffClass = nullptr;
	for (const FExItemSpawnEntry& Entry : CachedSpawnTable->BuffEntries)
	{
		if (Entry.ItemDefinition && Entry.ItemDefinition->PickupActorClass)
		{
			BuffClass = Entry.ItemDefinition->PickupActorClass;
			break;
		}
	}
	if (!BuffClass) return;

	// §3.4 Plan 기반 장애물 질의
	FExObstacleContext ObstCtx;
	const float QueryDist = Chunk->GetActorLocation().X + AtDistance;
	const float QueryRadius = 200.f;
	for (const FExSpawnPlan& Op : ObstaclePlan)
	{
		if (FMath::Abs(Op.WorldLocation.X - QueryDist) < QueryRadius)
		{
			ObstCtx.bHasObstacle = true;
			ObstCtx.ObstacleType = (EExObstacleType)Op.ObstacleTypeRaw;
			ObstCtx.ObstacleTopZ = Op.WorldLocation.Z + 100.f;
			ObstCtx.ObstacleBottomZ = Op.WorldLocation.Z;
			ObstCtx.bCanClimbOver = (ObstCtx.ObstacleType == EExObstacleType::Slide);
			break;
		}
	}

	FTransform LocalTrans = Chunk->GetLocalTransformAtDistance(AtDistance);

	// 레이터럴 오프셋을 스플라인 RightVector 기준으로 적용
	if (!FMath::IsNearlyZero(LateralOffset))
	{
		FVector SplineRight = LocalTrans.GetRotation().GetRightVector();
		LocalTrans.SetLocation(LocalTrans.GetLocation() + SplineRight * LateralOffset);
	}

	// 청크 World Transform 합성 → 진행 방향 포함된 월드 Transform
	FTransform GlobalTrans = LocalTrans * Chunk->GetActorTransform();

	// Z 계산: GlobalTrans의 Z를 기준으로 사용 (해당 위치의 실제 바닥 Z)
	const float ActualFloorZ = GlobalTrans.GetLocation().Z;
	const float PlacedZ = CalculateItemZ(ObstCtx, ActualFloorZ, 0.5f);

	FVector WorldPos = GlobalTrans.GetLocation();
	WorldPos.Z = PlacedZ;

	FExSpawnPlan Plan;
	Plan.OwnerSegmentIndex = Chunk->SegmentIndex;
	Plan.LocalPathOffset = AtDistance;
	Plan.ActorClass = BuffClass;
	Plan.PlacedZ = PlacedZ;
	Plan.WorldLocation = WorldPos;
	Plan.WorldRotation = GlobalTrans.Rotator(); // 청크 회전 포함된 월드 회전

	OutPlan.Add(Plan);
}
