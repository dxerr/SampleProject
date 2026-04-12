// Copyright ExFrameWork. All Rights Reserved.

#include "ExRunnerRuleBase.h"
#include "../GameModes/ExRunnerGameMode.h"
#include "Events/ExGameplayEventSubsystem.h"
#include "Engine/World.h"

void UExRunnerRuleBase::InitializeRule(AExRunnerGameMode* InGameMode)
{
	CachedGameMode = InGameMode;
	if (InGameMode)
	{
		CachedEventSubsystem = InGameMode->GetWorld()->GetSubsystem<UExGameplayEventSubsystem>();
	}
}

void UExRunnerRuleBase::ActivateRule()
{
	// 서브클래스에서 구현
}

void UExRunnerRuleBase::DeactivateRule()
{
	// 서브클래스에서 구현
}

void UExRunnerRuleBase::TickRule(float DeltaTime)
{
	// 서브클래스에서 구현 (bTickEnabled=true인 룰만 호출됨)
}
