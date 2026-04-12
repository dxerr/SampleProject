// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExRunnerRuleBase.h"
#include "ExRunnerRule_DistanceGoal.generated.h"

/**
 * UExRunnerRule_DistanceGoal
 * 목표 거리 달성 룰 — GameState::CurrentPathDistance가 GoalDistance 이상이면 GoalReached 발동
 */
UCLASS(BlueprintType, EditInlineNew)
class EXRUNNERPLAYRUNTIME_API UExRunnerRule_DistanceGoal : public UExRunnerRuleBase
{
	GENERATED_BODY()

public:
	UExRunnerRule_DistanceGoal();

	/** 달성 목표 거리 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule|DistanceGoal", meta = (ClampMin = "1.0"))
	float GoalDistance = 10000.f;

	virtual void ActivateRule() override;
	virtual void TickRule(float DeltaTime) override;
};
