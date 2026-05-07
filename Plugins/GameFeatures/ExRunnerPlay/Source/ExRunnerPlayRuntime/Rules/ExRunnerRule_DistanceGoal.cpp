// Copyright ExFrameWork. All Rights Reserved.

#include "ExRunnerRule_DistanceGoal.h"
#include "../GameModes/ExRunnerGameMode.h"
#include "../GameStates/ExRunnerGameState.h"
#include "../Tags/ExRunnerTags.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuleDistanceGoal, Log, All);

UExRunnerRule_DistanceGoal::UExRunnerRule_DistanceGoal()
{
	bTickEnabled = true;
	TriggerTag   = TAG_Rule_GoalReached;
}

void UExRunnerRule_DistanceGoal::ActivateRule()
{
	Super::ActivateRule();
	UE_LOG(LogRuleDistanceGoal, Log, TEXT("[Rule_DistanceGoal] 활성화 — 목표 거리: %.1f cm"), GoalDistance);
}

void UExRunnerRule_DistanceGoal::TickRule(float DeltaTime)
{
	if (!CachedGameMode) return;

	const AExRunnerGameState* GS = CachedGameMode->GetWorld()->GetGameState<AExRunnerGameState>();
	if (!GS) return;

	if (GS->CurrentPathDistance >= GoalDistance)
	{
		UE_LOG(LogRuleDistanceGoal, Log, TEXT("[Rule_DistanceGoal] 목표 달성! (%.1f / %.1f cm)"),
			GS->CurrentPathDistance, GoalDistance);
		OnRuleTriggered.Broadcast(TriggerTag, nullptr, this);
	}
}
