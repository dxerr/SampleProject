// Copyright ExFrameWork. All Rights Reserved.

#include "ExRunnerGameState.h"
#include "../Components/ExPathManager.h"
#include "Net/UnrealNetwork.h"
#include "../Components/ExChunkSpawner.h"
#include "../Components/ExObstacleManager.h"
#include "../Components/ExRunnerItemManager.h"
#include "../Player/ExRunnerPlayerState.h"
#include "../Data/ExRunnerConfig.h"
#include "Subsystems/ExDataCenterSubsystem.h"
#include "Engine/GameInstance.h"

AExRunnerGameState::AExRunnerGameState()
{
	// PathManager 생성
	PathManager = CreateDefaultSubobject<UExPathManager>(TEXT("PathManager"));
	
	// GameState에서 컴포넌트 리플리케이션을 위해 Replicates 활성화
	bReplicates = true;
	if (PathManager)
	{
		PathManager->SetIsReplicated(true);
	}

	// 로컬 기반으로 각 클라이언트에서 구동될 스포너들
	ChunkSpawner = CreateDefaultSubobject<UExChunkSpawner>(TEXT("ChunkSpawner"));
	ObstacleManager = CreateDefaultSubobject<UExObstacleManager>(TEXT("ObstacleManager"));
	ItemManager = CreateDefaultSubobject<UExRunnerItemManager>(TEXT("ItemManager"));
	
	// GameState 자체가 Tick을 통해 Lead/Tail 거리를 갱신하도록 설정
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AExRunnerGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AExRunnerGameState, CurrentPathDistance);
	DOREPLIFETIME(AExRunnerGameState, RealPlayerPathDistance);
	DOREPLIFETIME(AExRunnerGameState, RemainingTimeSeconds);
	DOREPLIFETIME(AExRunnerGameState, GameOverReason);

	// JIP 및 동기화 변수들
	DOREPLIFETIME(AExRunnerGameState, LeadDistance);
	DOREPLIFETIME(AExRunnerGameState, TailDistance);
	DOREPLIFETIME(AExRunnerGameState, SharedTrackSeed);
	DOREPLIFETIME(AExRunnerGameState, CurrentSegmentIndex);
	DOREPLIFETIME(AExRunnerGameState, SegmentStartDistance);
	DOREPLIFETIME(AExRunnerGameState, CleanupWatermark);
}

void AExRunnerGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		float MaxDist = 0.f;
		float MinDist = MAX_FLT;
		bool bAnyPlayerFound = false;

		// PlayerState 기반으로 리드/테일 계산
		for (APlayerState* PS : PlayerArray)
		{
			if (AExRunnerPlayerState* RunnerPS = Cast<AExRunnerPlayerState>(PS))
			{
				bAnyPlayerFound = true;
				float Dist = RunnerPS->ServerAuthPathDistance;
				if (Dist > MaxDist) MaxDist = Dist;
				if (Dist < MinDist) MinDist = Dist;
			}
		}

		if (bAnyPlayerFound)
		{
			LeadDistance = MaxDist;
			TailDistance = MinDist;
		}
		else
		{
			// 플레이어가 없으면 거리가 정체됨
			LeadDistance = 0.f;
			TailDistance = 0.f;
		}
	}
}

void AExRunnerGameState::SetRemainingTimeSeconds(int32 NewSeconds)
{
	RemainingTimeSeconds = NewSeconds;
	// 서버 자신도 델리게이트를 받도록 직접 브로드캐스트
	OnRemainingTimeChanged.Broadcast(NewSeconds);
}

void AExRunnerGameState::SetGameOverReason(EExRunnerGameOverReason NewReason)
{
	GameOverReason = NewReason;
	// 서버 자신도 델리게이트를 받도록 직접 브로드캐스트
	OnGameOverReasonChanged.Broadcast(NewReason);
}

void AExRunnerGameState::OnRep_RemainingTime()
{
	// OnRep은 UI 로직을 포함하지 않는다 — ViewModel이 구독
	OnRemainingTimeChanged.Broadcast(RemainingTimeSeconds);
}

void AExRunnerGameState::OnRep_GameOverReason()
{
	// OnRep은 UI 로직을 포함하지 않는다 — ViewModel이 구독
	OnGameOverReasonChanged.Broadcast(GameOverReason);
}

void AExRunnerGameState::OnRep_SharedTrackSeed()
{
	// 클라이언트 측에서 시드를 수신하면 (멀티플레이 접속)
	// 오프라인 로컬 스포너들을 초기화하여 서버와 동일한 맵을 구축하기 시작합니다.

	// PathManager 초기화 (클라이언트)
	if (PathManager)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UExDataCenterSubsystem* DC = GI->GetSubsystem<UExDataCenterSubsystem>())
			{
				PathManager->RunnerConfig = DC->GetConfig<UExRunnerConfig>();
			}
		}
		PathManager->InitializePath(FVector::ZeroVector, FRotator::ZeroRotator);
	}

	if (ChunkSpawner)
	{
		ChunkSpawner->InitializeSpawner();

		if (ObstacleManager && ItemManager)
		{
			ChunkSpawner->SetManagers(ObstacleManager, ItemManager);
			ObstacleManager->BindToSpawner(ChunkSpawner);
			ItemManager->BindToSpawner(ChunkSpawner);
		}
	}
}
