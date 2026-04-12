// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "../Rules/ExRunnerRuleBase.h"
#include "ExRunnerRuleConfig.generated.h"

/**
 * UExRunnerRuleConfig
 * 러너 게임 룰 조합을 에디터에서 정의하는 DataAsset
 *
 * Content Browser에서 DA_ExRule_* 에셋으로 생성하여, 
 * 각 게임모드 BP의 RuleManagerComponent에 참조로 연결한다.
 *
 * 예시:
 *  DA_ExRule_TimerMode    = [Rule_Timer, Rule_FallDeath]
 *  DA_ExRule_EndlessMode  = [Rule_FallDeath]
 *  DA_ExRule_DistanceMode = [Rule_DistanceGoal, Rule_FallDeath]
 */
UCLASS(BlueprintType)
class EXRUNNERPLAYRUNTIME_API UExRunnerRuleConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * 이 게임모드에서 활성화할 룰 목록
	 * EditInlineNew로 DataAsset 내부에서 룰 인스턴스를 직접 조립
	 */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Rules")
	TArray<TObjectPtr<UExRunnerRuleBase>> Rules;
};
