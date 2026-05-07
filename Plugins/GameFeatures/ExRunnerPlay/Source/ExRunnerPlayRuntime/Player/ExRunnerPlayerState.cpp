// Copyright ExFrameWork. All Rights Reserved.

#include "Player/ExRunnerPlayerState.h"
#include "Net/UnrealNetwork.h"

AExRunnerPlayerState::AExRunnerPlayerState()
{
	ServerAuthPathDistance = 0.0f;
	ClientPredictedPathDistance = 0.0f;
}

void AExRunnerPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AExRunnerPlayerState, ServerAuthPathDistance);
	DOREPLIFETIME(AExRunnerPlayerState, EliminationReason);
}

void AExRunnerPlayerState::UpdatePathDistance(float NewDistance)
{
	if (HasAuthority())
	{
		ServerAuthPathDistance = NewDistance;
	}
}

void AExRunnerPlayerState::SetEliminationReason(EExRunnerGameOverReason NewReason)
{
	if (HasAuthority())
	{
		EliminationReason = NewReason;
		// 서버도 로컬 플레이어일 수 있으므로(Host) OnRep 직접 호출
		OnRep_EliminationReason();
	}
}

void AExRunnerPlayerState::OnRep_EliminationReason()
{
	// 로컬 컨트롤러(자기 자신)인 경우에만 델리게이트 브로드캐스트
	if (APlayerController* PC = GetPlayerController())
	{
		if (PC->IsLocalController())
		{
			OnLocalPlayerEliminated.Broadcast(EliminationReason);
		}
	}
}
