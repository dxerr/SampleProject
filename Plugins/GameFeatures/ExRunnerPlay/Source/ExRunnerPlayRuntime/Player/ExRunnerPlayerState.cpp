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
}

void AExRunnerPlayerState::UpdatePathDistance(float NewDistance)
{
	if (HasAuthority())
	{
		ServerAuthPathDistance = NewDistance;
	}
}
