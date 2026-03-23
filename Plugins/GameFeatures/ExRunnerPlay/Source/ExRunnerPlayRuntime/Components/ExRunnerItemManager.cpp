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
		// 스폰 확률에 탈락하여 라인이 끊기면 이월 데이터와 뱀 패턴을 0으로 초기화0
		PersistentNextCoinDistance = 0.f;
		PersistentSnakeOffset = 0.f;
	}

	// 코인 라인 중 버프 삽입 확률
	if (bCoinLineSpawned && FMath::FRand() < SpawnTable->BuffSpawnProbability)
	{
		float BuffDistance = FMath::FRandRange(SafeStart, SafeEnd);
		float SnakeOffset = SpawnTable->bUseSnakePattern ? PersistentSnakeOffset : 0.f;
		SpawnBuffItem(TargetChunk, ObstacleManager, BuffDistance, SnakeOffset);
	}
	// 코인 라인 없는 청크: 단독 버프 배치
	else if (!bCoinLineSpawned && FMath::FRand() < SpawnTable->BuffSoloSpawnProbability)
	{
		float BuffDistance = FMath::FRandRange(SafeStart, SafeEnd);
		float SnakeOffset = SpawnTable->bUseSnakePattern ? PersistentSnakeOffset : 0.f;
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

	const int32 CoinCount = FMath::RandRange(SpawnTable->MinCoinsPerLine, SpawnTable->MaxCoinsPerLine);
	const float Spacing = SpawnTable->CoinSpacing;
	float CurrentDistance = StartDistance;

	// 청크의 실제 단위 폭을 기반으로 안전한 이동 범위 도출
	float HalfWidth = 400.f;
	if (Chunk)
	{
		FBox BaseBounds = Chunk->GetFloorBounds();
		HalfWidth = BaseBounds.GetExtent().Y * Chunk->GetActorScale3D().Y;
	}
	float SafeMargin = 70.f; // 코인의 충돌 크기(반지름) 여유분
	float RealMaxOffset = FMath::Max(0.f, HalfWidth - SafeMargin);
	
	// 기획자가 설정한 제한값과 청크의 실제 폭 제한값 중 작은 쪽(안전한 쪽) 선택
	float EffectiveMaxOffset = FMath::Min(SpawnTable->MaxLateralOffset, RealMaxOffset);

	for (int32 i = 0; i < CoinCount && CurrentDistance < EndDistance; ++i)
	{
		// 코인 라인 끊김 확률
		if (i > 0 && FMath::FRand() < SpawnTable->CoinLineBreakProbability)
		{
			CurrentDistance += Spacing * 2.f; // 끊김 시 간격 2배
			continue;
		}

		// 코인 선택
		const UExItemDefinition* CoinDef = SpawnTable->SelectRandomCoin(0.f); // TODO: 현재 속도 연동
		if (!CoinDef)
		{
			CurrentDistance += Spacing;
			continue;
		}

		// 뱀(Snake) 패턴 로직에 의한 지속적 좌우 위치 업데이트 (상태 저장)
		if (SpawnTable->bUseSnakePattern && EffectiveMaxOffset > 0.f)
		{
			// 현재 방향으로 드리프트 이동
			PersistentSnakeOffset += PersistentSnakeDir * SpawnTable->LateralDriftPerCoin;

			// 설정된(또는 청크의) 최대 범위를 넘어가면 강제 반전. (당분간 반대 방향 유지)
			if (PersistentSnakeOffset >= EffectiveMaxOffset)
			{
				PersistentSnakeOffset = EffectiveMaxOffset;
				PersistentSnakeDir = -1.f;
			}
			else if (PersistentSnakeOffset <= -EffectiveMaxOffset)
			{
				PersistentSnakeOffset = -EffectiveMaxOffset;
				PersistentSnakeDir = 1.f;
			}
		}
		else
		{
			PersistentSnakeOffset = 0.f;
		}

		// 청크 표면 트랜스폼 획득 및 월드 스페이스 변환 (곡선 청크 대응)
		FTransform LocalTransform = Chunk->GetLocalTransformAtDistance(CurrentDistance);
		
		// 스레드밀의 X축(Forward) 기준으로 측면(Y축) 오프셋을 Local Transform에 적용
		if (SpawnTable->bUseSnakePattern && !FMath::IsNearlyZero(PersistentSnakeOffset))
		{
			FVector SplineRightVector = LocalTransform.GetRotation().GetRightVector();
			LocalTransform.SetLocation(LocalTransform.GetLocation() + SplineRightVector * PersistentSnakeOffset);
		}
		
		// [수정] FRotator 덧셈은 Gimbal Lock 및 3차원 축 왜곡을 발생시키므로 행렬 곱을 사용해야 함
		FTransform GlobalTransform = LocalTransform * Chunk->GetActorTransform();
		FVector SpawnLocation = GlobalTransform.GetLocation();
		FRotator SpawnRotation = GlobalTransform.Rotator();

		// 실제 글로벌 PathDistance 매핑 (Chunk->PathDistance는 세그먼트의 중심임)
		float GlobalPathDistance = Chunk->PathDistance - (Chunk->ChunkLength * 0.5f) + CurrentDistance;

		// 장애물 질의 및 Z축 결정
		float AlphaInGap = 0.f;
		FExObstacleContext Context;

		if (ObstacleManager)
		{
			ObstacleManager->QueryObstacleAtDistance(GlobalPathDistance, Spacing * 0.5f, Context);
			
			// Gap 장애물인 경우 포물선 비례값(AlphaInGap) 계산
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

		// 무조건 CalculateItemZ를 거쳐서 파묻힘 방지 Z Offset을 포함하도록 함
		float FinalZ = CalculateItemZ(Context, SpawnLocation.Z, AlphaInGap);

		SpawnLocation.Z = FinalZ;

		// 아이템 스폰 및 청크에 부착
		AExItemPickupBase* SpawnedItem = SpawnItem(CoinDef, FTransform(SpawnRotation, SpawnLocation));
		if (SpawnedItem)
		{
			SpawnedItem->AttachToActor(Chunk, FAttachmentTransformRules::KeepWorldTransform);
		}

		CurrentDistance += Spacing;
	}

	// 청크를 다 채워서 루프가 끝났다면, 완벽한 간격 유지를 위해 남은 거리를 이월
	if (CurrentDistance >= EndDistance)
	{
		PersistentNextCoinDistance = CurrentDistance - Chunk->ChunkLength;
	}
	else
	{
		// 청크를 다 채우기 전에 CoinCount(갯수 제한)를 다 소모해버렸다면 라인 종료
		PersistentNextCoinDistance = 0.f;
		PersistentSnakeOffset = 0.f;
	}
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
