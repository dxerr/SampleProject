/**
 * @file ExChunkSpawner.cpp
 * @brief 청크 스폰 및 오브젝트 풀 관리 컴포넌트 구현
 * @details 러너 게임에서 무한 맵 생성을 위한 청크 풀링 시스템
 * 
 * Copyright ExFrameWork. All Rights Reserved.
 */

#include "ExChunkSpawner.h"
#include "../Actors/ExFloorChunk.h"
#include "ExPathManager.h"
#include "../GameStates/ExRunnerGameState.h"
#include "../Data/ExRunnerConfig.h"
#include "../Components/ExObstacleManager.h"
#include "../Components/ExRunnerItemManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/BoxComponent.h"
#include "Subsystems/ExDataCenterSubsystem.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogExChunkSpawner, Log, All);

UExChunkSpawner::UExChunkSpawner()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// --- 청크 스포너 구현 ---

void UExChunkSpawner::BeginPlay()
{
	Super::BeginPlay();
	
	if (UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		if (UExDataCenterSubsystem* DC = GI->GetSubsystem<UExDataCenterSubsystem>())
		{
			RunnerConfig = DC->GetConfig<UExRunnerConfig>();
		}
	}

	bool bShouldUsePooling = false;
	int32 ExpectedPoolSize = 5;
	float StartX = 0.f;

	if (RunnerConfig.IsValid())
	{
		bShouldUsePooling = RunnerConfig->ChunkSpawn.bUsePooling;
		ExpectedPoolSize = RunnerConfig->ChunkSpawn.InitialPoolSize;
		StartX = RunnerConfig->ChunkSpawn.SpawnStartX;
	}

	// 초기 풀 크기만큼 청크 생성 보관 (풀링 사용 시에만)
	if (bShouldUsePooling)
	{
		for (int32 i = 0; i < ExpectedPoolSize; ++i)
		{
			AExFloorChunk* NewChunk = CreateNewChunk();
			if (NewChunk)
			{
				ReturnChunkToPool(NewChunk);
			}
		}
	}
	
	// 초기 스폰 위치 설정
	NextSpawnX = StartX;
}

void UExChunkSpawner::InitializeSpawner()
{
	// 기존 청크 모두 정리
	ClearAllChunks();
	
	// GameState에서 PathManager 가져오기
	UExPathManager* PM = nullptr;
	if (UWorld* World = GetWorld())
	{
		if (AExRunnerGameState* GS = World->GetGameState<AExRunnerGameState>())
		{
			PM = GS->PathManager;
		}
	}

	// [Fix] 초기 세그먼트(0,0,0 위치) 스폰
	// PathManager가 InitializePath()에서 생성한 첫 세그먼트는 
	// GenerateNextSegment() 호출 시 건너뛰어지므로, 여기서 명시적으로 스폰
	int32 SpawnCount = 0;
	if (PM && PM->GetSegments().Num() > 0)
	{
		// 0번 세그먼트 스폰 (보통 플레이어 시작 위치)
		SpawnNextChunk(0);
		SpawnCount++;
	}

	// 나머지 청크 채우기 (새 세그먼트 생성)
	int32 MaxChunksTarget = 10;
	if (RunnerConfig.IsValid())
	{
		MaxChunksTarget = RunnerConfig->ChunkSpawn.MaxActiveChunks;
	}

	for (int32 i = SpawnCount; i < MaxChunksTarget; ++i)
	{
		SpawnNextChunk(-1);
	}
}

void UExChunkSpawner::SetManagers(UExObstacleManager* InObstacleManager, UExRunnerItemManager* InItemManager)
{
	CachedObstacleManager = InObstacleManager;
	CachedItemManager = InItemManager;
}

AExFloorChunk* UExChunkSpawner::SpawnNextChunk(int32 OverrideSegmentIndex)
{
	AExFloorChunk* Chunk = GetChunkFromPool();
	if (!Chunk)
	{
		return nullptr;
	}

	// ── PathManager 연동: 경로 기반 스폰 ──

	// GameState에서 PathManager 가져오기
	UExPathManager* PM = nullptr;
	if (UWorld* World = GetWorld())
	{
		if (AExRunnerGameState* GS = World->GetGameState<AExRunnerGameState>())
		{
			PM = GS->PathManager;
		}
	}

	if (PM && PM->RunnerConfig.IsValid())
	{
		// ── 경로 기반 배치 ──
		
		const FExPathSegment* CurrentSeg = nullptr;

		// 1) 세그먼트 확보 (Override 또는 신규 생성)
		if (OverrideSegmentIndex >= 0)
		{
			// 특정 인덱스 세그먼트 사용 (초기화용)
			if (PM->GetSegments().IsValidIndex(OverrideSegmentIndex))
			{
				CurrentSeg = &PM->GetSegments()[OverrideSegmentIndex];
			}
			else
			{
				// 인덱스 오류 시 그냥 다음 생성? 혹은 실패?
				// 안전하게 다음 생성 시도
				CurrentSeg = &PM->GenerateNextSegment();
			}
		}
		else
		{
			// 다음 세그먼트 생성
			CurrentSeg = &PM->GenerateNextSegment();
		}

		if (!CurrentSeg) return nullptr;

		const FExPathSegment& Seg = *CurrentSeg;

		// 2) 스폰 위치/회전 = 세그먼트 중심점 (Pivot = Center)
		// SplineMesh가 Center Pivot으로 생성되므로, Actor는 세그먼트의 **중간**에 위치해야 함
		// Alpha=0.5 지점이 세그먼트의 중심
		FVector SpawnPos = Seg.GetPositionAtAlpha(0.5f);
		FRotator SpawnRot = Seg.GetRotationAtAlpha(0.5f);

		// 3) 청크 활성화 (회전 포함)
		// ★ 중요: 커브(나선형)의 경우, Actor가 Pitch를 가지면 로컬 좌표계가 기울어져 
		//    ApplyCurve의 Z축 변위와 중첩되어 의도치 않은 비틀림 발생.
		//    따라서 커브일 때는 Actor 회전을 수평(Yaw Only)으로 고정하고, 
		//    높이 변화는 온전히 ApplyCurve의 Z 오프셋으로 처리.
		if (Seg.Type != EExPathSegmentType::Straight)
		{
			SpawnRot.Pitch = 0.f;
			SpawnRot.Roll = 0.f;
		}

		Chunk->ActivateChunkWithRotation(SpawnPos, SpawnRot);

		// 4) 경로 정보 설정 (Actor의 중심 거리)
		const float MidDistance = Seg.CumulativeStartDistance + (Seg.ArcLength * 0.5f);
		Chunk->PathDistance = MidDistance; // 중심 거리 저장
		Chunk->SegmentType = Seg.Type;
		Chunk->ChunkLength = Seg.ArcLength; // [Fix] 세그먼트의 실제 호 길이(ArcLength) 동기화

		// 5) 모든 청크에 Spline Mesh 적용 (Straight 포함 통일감 부여)
		// Straight: Angle=0, Radius=0. Radius는 무시됨.
		const bool bIsLeft = (Seg.Type == EExPathSegmentType::CurveLeft);
		
		Chunk->ApplyCurve(Seg.CurveAngle, Seg.CurveRadius, 
			PM->RunnerConfig->Curve.SplineSegmentCount, bIsLeft, Seg.HeightOffset);

		UE_LOG(LogExChunkSpawner, Log, TEXT("경로 기반 스폰: [%s] Type=%d, PathDist=%.1f (Mid), Pos=%s"),
			*Chunk->GetName(), (int32)Seg.Type, MidDistance, *SpawnPos.ToString());
	}
	else
	{
		// ── 레거시 직선 배치 (기존 로직) ──
		float ConfigSpawnStartX = 0.f;
		float ConfigChunkSpacing = 1000.f;
		if (RunnerConfig.IsValid())
		{
			ConfigSpawnStartX = RunnerConfig->ChunkSpawn.SpawnStartX;
			ConfigChunkSpacing = RunnerConfig->ChunkSpawn.ChunkSpacing;
		}

		float SpawnX = ConfigSpawnStartX;
		if (ActiveChunks.Num() > 0)
		{
			AExFloorChunk* LastChunk = ActiveChunks.Last();
			if (IsValid(LastChunk))
			{
				SpawnX = LastChunk->GetActorLocation().X + ConfigChunkSpacing;
			}
		}

		FVector SpawnLocation(SpawnX, 0.f, 0.f);
		Chunk->ActivateChunk(SpawnLocation);
		Chunk->SegmentType = EExPathSegmentType::Straight;
		Chunk->PathDistance = 0.f;
		Chunk->ChunkLength = 1000.f; // 레거시 직선 길이 (1000)
	}

	// 활성 목록에 추가
	ActiveChunks.Add(Chunk);

	// 델리게이트를 통해 청크 생성 알림
	if (OnChunkSpawned.IsBound())
	{
		OnChunkSpawned.Broadcast(Chunk);
	}

	// [중앙 제어] 명시적인 순서로 매니저 호출 (장애물 먼저, 그 이후 아이템)
	// ★ 개발 중 Hot Reload(Re-instancing)로 인해 포인터가 stale해지는 경우를 대비해 유효성 체크 강화
	if (!CachedObstacleManager || !CachedItemManager)
	{
		CachedObstacleManager = GetOwner()->FindComponentByClass<UExObstacleManager>();
		CachedItemManager = GetOwner()->FindComponentByClass<UExRunnerItemManager>();
	}

	if (CachedObstacleManager)
	{
		CachedObstacleManager->SpawnObstaclesOnChunk(Chunk, 0.f, Chunk->ChunkLength, false);
	}

	if (CachedItemManager)
	{
		CachedItemManager->SpawnItemsOnChunk(Chunk, CachedObstacleManager);
	}

	return Chunk;
}

void UExChunkSpawner::ReturnChunkToPool(AExFloorChunk* Chunk)
{
	if (!IsValid(Chunk))
	{
		return;
	}
	
	// 비활성화 및 소멸 알림
	Chunk->DeactivateChunk();
	ActiveChunks.Remove(Chunk);
	
	if (OnChunkDespawned.IsBound())
	{
		OnChunkDespawned.Broadcast(Chunk);
	}

	// 풀링 사용 안 하면 아예 액터 소멸
	bool bShouldUsePooling = RunnerConfig.IsValid() ? RunnerConfig->ChunkSpawn.bUsePooling : false;
	if (!bShouldUsePooling)
	{
		Chunk->Destroy();
		return;
	}

	// 풀에 반환 (앞에 삽입 - FIFO 방식)
	// Pop()은 뒤에서 꺼내므로, 앞에 삽입하면 최소 1사이클 딜레이 확보
	ChunkPool.Insert(Chunk, 0);
}

void UExChunkSpawner::ClearAllChunks()
{
	for (AExFloorChunk* Chunk : ActiveChunks)
	{
		if (IsValid(Chunk))
		{
			ReturnChunkToPool(Chunk);
		}
	}
	ActiveChunks.Empty();
}

AExFloorChunk* UExChunkSpawner::GetLatestChunk() const
{
	if (ActiveChunks.Num() > 0)
	{
		return ActiveChunks.Last();
	}
	return nullptr;
}


void UExChunkSpawner::OnChunkReachedKillZ(AExFloorChunk* Chunk)
{
	if (IsValid(Chunk))
	{
		// [Fix] 순서 변경: 먼저 스폰 → 그 다음 반환
		// 이렇게 하면 삭제된 청크가 즉시 재사용되는 것을 방지
		
		// 1️⃣ 먼저 새 청크 스폰 (풀에서 다른 청크 사용)
		SpawnNextChunk();
		
		// 2️⃣ 그 다음 삭제된 청크를 풀로 반환
		ReturnChunkToPool(Chunk);
	}
}

AExFloorChunk* UExChunkSpawner::GetChunkFromPool()
{
	// 오브젝트 풀링 미사용 시 무조건 바로 새로 스폰
	bool bShouldUsePooling = RunnerConfig.IsValid() ? RunnerConfig->ChunkSpawn.bUsePooling : false;
	if (!bShouldUsePooling)
	{
		return CreateNewChunk();
	}

	// FIFO (First-In, First-Out) 방식
	// Insert(0)으로 앞에 추가, Pop()으로 뒤에서 제거
	// 이를 통해 반환된 청크가 최소 1사이클 이상 대기 후 재사용됨
	if (ChunkPool.Num() > 0)
	{
		AExFloorChunk* Chunk = ChunkPool.Pop();
		
		if (IsValid(Chunk))
		{
			return Chunk;
		}
	}
	
	// 풀이 비었으면 새로 생성
	return CreateNewChunk();
}

AExFloorChunk* UExChunkSpawner::CreateNewChunk()
{
	TSubclassOf<AExFloorChunk> TargetChunkClass = ChunkClass;
	if (RunnerConfig.IsValid() && RunnerConfig->ChunkSpawn.ChunkClass)
	{
		TargetChunkClass = RunnerConfig->ChunkSpawn.ChunkClass;
	}

	if (!TargetChunkClass)
	{
		UE_LOG(LogExChunkSpawner, Error, TEXT("ChunkClass is not set! (Check RunnerConfig or Spawner settings)"));
		return nullptr;
	}
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	AExFloorChunk* NewChunk = GetWorld()->SpawnActor<AExFloorChunk>(TargetChunkClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (NewChunk)
	{
		// KillZ 도달 델리게이트 바인딩
		NewChunk->OnChunkReachedKillZ.AddDynamic(this, &UExChunkSpawner::OnChunkReachedKillZ);
	}
	
	return NewChunk;
}

