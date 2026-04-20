// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Base/ExPresetDataAsset.h"
#include "../Rules/ExRunnerRuleBase.h"
#include "ExRunnerRuleConfig.generated.h"

/**
 * UExRunnerRuleConfig — 러너 게임 룰 조합 Preset DA
 *
 * 모드별 룰 조합(Timer, FallDeath, DistanceGoal 등)을 정의한다.
 * DataCenter에서 PresetTag를 키로 조회하여 해당 모드의 룰 셋을 가져온다.
 *
 * [DataCenter 분류]: Preset — 같은 태그를 다른 Preset 타입과 공유해도 충돌 없음
 *
 * [에디터 에셋 명명 예시]
 *  DA_ExPreset_TimerMode    → PresetTag: Ex.Mode.Timer
 *  DA_ExPreset_EndlessMode  → PresetTag: Ex.Mode.Endless
 *  DA_ExPreset_DistanceMode → PresetTag: Ex.Mode.Distance
 *
 * [기존 참조 호환]
 *  TObjectPtr<UExRunnerRuleConfig> 직접 참조는 계속 동작한다.
 *  DataCenter 조회는 선택적으로 단계적 전환 가능.
 */
UCLASS(BlueprintType)
class EXRUNNERPLAYRUNTIME_API UExRunnerRuleConfig : public UExPresetDataAsset
{
	GENERATED_BODY()

public:

	/**
	 * 이 게임모드에서 활성화할 룰 목록.
	 * Instanced 편집으로 DA 내부에서 룰 인스턴스를 직접 조립한다.
	 * 룰은 Stateless로 설계되어야 하며 런타임 상태를 저장하지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Rules")
	TArray<TObjectPtr<UExRunnerRuleBase>> Rules;

#if WITH_EDITOR
	/**
	 * 저장 시점 유효성 검증.
	 * - PresetTag 비어있으면 에러 (Super에서 처리)
	 * - Rules 배열이 비어있으면 경고 (룰 없는 모드는 의도적 비허용)
	 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
