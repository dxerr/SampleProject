// Copyright ExFrameWork. All Rights Reserved.

#include "ExRunnerItemManager.h"
#include "ExItemSystemTypes.h"
#include "ExItemDefinition.h"
#include "ExItemPickupBase.h"
#include "ExRunnerItemSpawnTable.h"
#include "ExObstacleManager.h"
#include "ExFloorChunk.h"
#include "ExChunkSpawner.h"
#include "Curves/CurveFloat.h"
#include "Components/SphereComponent.h"

UExRunnerItemManager::UExRunnerItemManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UExRunnerItemManager::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogExItemSystem, Log, TEXT("[ExRunnerItemManager] BeginPlay — SpawnTable: %s"),
		SpawnTable ? *SpawnTable->GetName() : TEXT("미할당"));
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
		if (Context.bCanClimbOver && FMath::FRand() < SlideTopPlacementRatio)
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

// ── 중앙 제어 스폰 ──

void UExRunnerItemManager::SpawnItemsOnChunk(AExFloorChunk* TargetChunk, UExObstacleManager* ObstacleManager)
{
	if (!TargetChunk)
	{
		UE_LOG(LogExItemSystem, Warning, TEXT("[ExRunnerItemManager] SpawnItemsOnChunk: TargetChunk가 null입니다!"));
		return;
	}

	if (!SpawnTable)
	{
		UE_LOG(LogExItemSystem, Warning, TEXT("[ExRunnerItemManager] SpawnTable이 할당되지 않았습니다! (아이템 스폰 건너뜀)"));
		return;
	}

	const float ChunkLength = TargetChunk->ChunkLength;
	
	// [수정] 기존에는 청크 양 끝 200cm를 무조건 비워버렸기 때문에 청크 사이에 거대한 틈이 강제되었습니다.
	// 이제 청크의 시작 지점은 이전 청크에서 이월된 코인 스폰 잔여 오프셋을 사용하고, 끝까지 꽉 채웁니다.
	const float SafeStart = PersistentNextCoinDistance;
	const float SafeEnd = ChunkLength;

	if (SafeStart >= SafeEnd)
	{
		// 이월된 간격이 이번 청크 길이보다도 길면 다음 청크로 넘김
		PersistentNextCoinDistance -= ChunkLength;
		return; 
	}

	bool bCoinLineSpawned = false;

	// 코인 라인 스폰 확률 판정
	if (FMath::FRand() < SpawnTable->CoinLineSpawnProbability)
	{
		SpawnCoinLine(TargetChunk, ObstacleManager, SafeStart, SafeEnd);
		bCoinLineSpawned = true;
	}
	else 
	{
		// 스폰 확률에 탈락하여 라인이 끊기면 이월 데이터와 뱀 패턴을 초기화
		PersistentNextCoinDistance = 0.f;
		CurrentLaneYOffset = 0.f;
		RemainingCoinsInCurrentLane = 0;
	}

	// 코인 라인 없는 청크: 단독 버프 배치
	if (!bCoinLineSpawned && FMath::FRand() < SpawnTable->BuffSoloSpawnProbability)
	{
		float BuffDistance = FMath::FRandRange(SafeStart, SafeEnd);
		float SnakeOffset = SpawnTable->bUseSnakePattern ? CurrentLaneYOffset : 0.f;
		SpawnBuffItem(TargetChunk, ObstacleManager, BuffDistance, SnakeOffset);
	}
}

// ── 코인 라인 배치 ──

void UExRunnerItemManager::SpawnCoinLine(AExFloorChunk* Chunk, UExObstacleManager* ObstacleManager, float StartDistance, float EndDistance)
{
	if (!SpawnTable || SpawnTable->CoinEntries.Num() == 0)
	{
		return;
	}

	const float Spacing = SpawnTable->CoinSpacing;
	float CurrentDistance = StartDistance;

	// 바닥의 실제 Y 범위 구해 LaneWidth 산출 (레벨 디자인상 3등분 폭)
	float LaneWidth = 100.f;
	if (Chunk)
	{
		float TotalWidth = Chunk->GetFloorBounds().GetSize().Y * Chunk->GetActorScale3D().Y;
		LaneWidth = TotalWidth / 3.0f;
	}

	// 이번 청크(호출 주기) 내에서 스폰 가능한 최대 횟수를 예상
	int32 EstimatedSpawns = FMath::Max(1, FMath::CeilToInt((EndDistance - StartDistance) / Spacing));
	int32 MaxIterCount = 100; // 절대 무한루프 방지
	
	// 라인 중 버프 교체 삽입 (확률 판정 후 임의의 슬롯 인덱스 지정)
	int32 BuffReplaceIndex = -1;
	if (FMath::FRand() < SpawnTable->BuffSpawnProbability)
	{
		BuffReplaceIndex = FMath::RandRange(0, EstimatedSpawns - 1);
	}

	for (int32 i = 0; i < MaxIterCount && CurrentDistance < EndDistance; ++i)
	{
		// 현재 레인의 잔여 갯수가 0이면 새로운 목표 레인 결정
		if (RemainingCoinsInCurrentLane <= 0)
		{
			// 지그재그 유도: 기존 레인과 다른 레인을 강제 무작위 추첨
			TArray<int32> PossibleLanes = {-1, 0, 1};
			PossibleLanes.Remove(PersistentTargetLane);
			PersistentTargetLane = PossibleLanes[FMath::RandRange(0, PossibleLanes.Num() - 1)];

			// 할당량 갱신
			RemainingCoinsInCurrentLane = FMath::RandRange(SpawnTable->MinCoinsPerLine, SpawnTable->MaxCoinsPerLine);
		}

		// 코인 라인 중간 끊김 판정
		if (i > 0 && FMath::FRand() < SpawnTable->CoinLineBreakProbability)
		{
			CurrentDistance += Spacing * 2.f; // 끊김 시 간격 2배
			
			// 끊김이 발생하면 대각선 뱀 패턴 스킵 후, 곧장 새 목표 레인으로 이동(스냅)
			CurrentLaneYOffset = PersistentTargetLane * LaneWidth;
			continue;
		}

		// 스폰할 아이템 정의서 결정 (교체형 버프 vs 코인)
		const UExItemDefinition* ItemDef = nullptr;
		if (i == BuffReplaceIndex && SpawnTable->BuffEntries.Num() > 0)
		{
			ItemDef = SpawnTable->SelectRandomBuff(0.f); 
		}
		else
		{
			ItemDef = SpawnTable->SelectRandomCoin(0.f);
		}

		if (!ItemDef)
		{
			CurrentDistance += Spacing;
			continue;
		}

		// 뱀(Snake) 패턴 보간 계산 (목표 레인을 향해 한 걸음씩 드리프트)
		if (SpawnTable->bUseSnakePattern)
		{
			float TargetY = PersistentTargetLane * LaneWidth;
			float DistanceToTarget = TargetY - CurrentLaneYOffset;
			
			if (FMath::Abs(DistanceToTarget) > KINDA_SMALL_NUMBER)
			{
				float Drift = SpawnTable->LateralDriftPerCoin;
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

		// 이동 중(보간 중)인 상황도 횟수 차감 포함
		RemainingCoinsInCurrentLane--;

		// 곡선 청크 대응 월드 트랜스폼 산출
		FTransform LocalTransform = Chunk->GetLocalTransformAtDistance(CurrentDistance);
		
		if (!FMath::IsNearlyZero(CurrentLaneYOffset))
		{
			FVector SplineRightVector = LocalTransform.GetRotation().GetRightVector();
			LocalTransform.SetLocation(LocalTransform.GetLocation() + SplineRightVector * CurrentLaneYOffset);
		}
		
		FTransform GlobalTransform = LocalTransform * Chunk->GetActorTransform();
		FVector SpawnLocation = GlobalTransform.GetLocation();
		FRotator SpawnRotation = GlobalTransform.Rotator();

		// 실제 글로벌 PathDistance
		float GlobalPathDistance = Chunk->PathDistance - (Chunk->ChunkLength * 0.5f) + CurrentDistance;

		// 장애물 질의를 통한 최종 Z 보정
		float AlphaInGap = 0.f;
		FExObstacleContext Context;

		if (ObstacleManager)
		{
			ObstacleManager->QueryObstacleAtDistance(GlobalPathDistance, Spacing * 0.5f, Context);
			
			if (Context.ObstacleType == EExObstacleType::Gap && Context.ObstacleBounds.IsValid)
			{
				float GapStart = Context.ObstacleBounds.Min.X;
				float GapEnd = Context.ObstacleBounds.Max.X;
				float GapLength = GapEnd - GapStart;
				if (GapLength > 0.f)
				{
					AlphaInGap = FMath::Clamp((SpawnLocation.X - GapStart) / GapLength, 0.f, 1.f);
				}
			}
		}

		SpawnLocation.Z = CalculateItemZ(Context, SpawnLocation.Z, AlphaInGap);

		AExItemPickupBase* SpawnedItem = SpawnItem(ItemDef, FTransform(SpawnRotation, SpawnLocation));
		if (SpawnedItem)
		{
			SpawnedItem->AttachToActor(Chunk, FAttachmentTransformRules::KeepWorldTransform);
		}

		CurrentDistance += Spacing;
	}

	// 청크를 다 채웠다면, 완벽한 간격 유지를 위해 남은 거리를 이월 (상태값 보존)
	if (CurrentDistance >= EndDistance)
	{
		PersistentNextCoinDistance = CurrentDistance - Chunk->ChunkLength;
	}
	else
	{
		// 이전에 EndDistance에 도달 못 한 건 MaxIter에 걸린 예외 스폰이므로 리셋
		PersistentNextCoinDistance = 0.f;
		CurrentLaneYOffset = 0.f;
		RemainingCoinsInCurrentLane = 0;
	}
}

float UExRunnerItemManager::GetCachedCoinRadius()
{
	// 캐싱된 값이 있으면 즉시 반환
	if (CachedCoinRadius > 0.f)
	{
		return CachedCoinRadius;
	}

	// 스폰 테이블의 첫 번째 코인 애셋 정보를 실시간으로 질의하여 반지름 획득
	if (SpawnTable && SpawnTable->CoinEntries.Num() > 0)
	{
		if (const UExItemDefinition* CoinDef = SpawnTable->CoinEntries[0].ItemDefinition)
		{
			if (TSubclassOf<AExItemPickupBase> PickupClass = CoinDef->PickupActorClass)
			{
				if (AExItemPickupBase* CDO = PickupClass.GetDefaultObject())
				{
					// CDO에서 구체 컴포넌트를 찾아 설정된 반지름을 가져옴
					if (const USphereComponent* Sphere = CDO->FindComponentByClass<USphereComponent>())
					{
						CachedCoinRadius = Sphere->GetUnscaledSphereRadius();
						UE_LOG(LogExItemSystem, Log, TEXT("[ExRunnerItemManager] Coin Radius Cached: %.2f (Margin will be %.2f)"), 
							CachedCoinRadius, CachedCoinRadius * 2.f);
						return CachedCoinRadius;
					}
				}
			}
		}
	}

	// 데이터가 없거나 로드 전인 경우 안전한 기본값 반환 (캐싱은 하지 않음)
	return 100.f;
}

// ── 버프 아이템 배치 ──

void UExRunnerItemManager::SpawnBuffItem(AExFloorChunk* Chunk, UExObstacleManager* ObstacleManager, float AtDistance, float LateralOffset)
{
	if (!SpawnTable || SpawnTable->BuffEntries.Num() == 0)
	{
		return;
	}

	const UExItemDefinition* BuffDef = SpawnTable->SelectRandomBuff(0.f); // TODO: 현재 속도 연동
	if (!BuffDef)
	{
		return;
	}

	// 청크 표면 트랜스폼 획득 및 월드 스페이스 변환
	FTransform LocalTransform = Chunk->GetLocalTransformAtDistance(AtDistance);

	if (!FMath::IsNearlyZero(LateralOffset))
	{
		FVector SplineRightVector = LocalTransform.GetRotation().GetRightVector();
		LocalTransform.SetLocation(LocalTransform.GetLocation() + SplineRightVector * LateralOffset);
	}

	FTransform GlobalTransform = LocalTransform * Chunk->GetActorTransform();
	FVector SpawnLocation = GlobalTransform.GetLocation();
	FRotator SpawnRotation = GlobalTransform.Rotator();

	// 실제 글로벌 PathDistance
	float GlobalPathDistance = Chunk->PathDistance - (Chunk->ChunkLength * 0.5f) + AtDistance;

	// 장애물 질의 및 Z축 결정
	FExObstacleContext Context;
	if (ObstacleManager)
	{
		ObstacleManager->QueryObstacleAtDistance(GlobalPathDistance, SpawnTable->MinDistanceFromObstacle, Context);
	}

	float FinalZ = CalculateItemZ(Context, SpawnLocation.Z, 0.5f);

	SpawnLocation.Z = FinalZ;

	AExItemPickupBase* SpawnedItem = SpawnItem(BuffDef, FTransform(SpawnRotation, SpawnLocation));
	if (SpawnedItem)
	{
		SpawnedItem->AttachToActor(Chunk, FAttachmentTransformRules::KeepWorldTransform);
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
