// Copyright ExFrameWork. All Rights Reserved.

#include "ExRunnerGameState.h"
#include "../Components/ExPathManager.h"
#include "Net/UnrealNetwork.h"

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
}

void AExRunnerGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AExRunnerGameState, CurrentPathDistance);
	DOREPLIFETIME(AExRunnerGameState, RealPlayerPathDistance);
	DOREPLIFETIME(AExRunnerGameState, RemainingTimeSeconds);
	DOREPLIFETIME(AExRunnerGameState, GameOverReason);
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
