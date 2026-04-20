// Copyright ExFrameWork. All Rights Reserved.

#include "Data/ExRunnerRuleConfig.h"
#include "Misc/DataValidation.h"

#if WITH_EDITOR
EDataValidationResult UExRunnerRuleConfig::IsDataValid(FDataValidationContext& Context) const
{
	// [공통 검증] PresetTag 비어있으면 에러 — 부모(UExPresetDataAsset)에서 처리
	EDataValidationResult Result = Super::IsDataValid(Context);

	// [추가 검증] Rules 배열이 비어있으면 경고
	if (Rules.IsEmpty())
	{
		Context.AddWarning(FText::Format(
			NSLOCTEXT("ExDataCenter", "EmptyRules",
				"[ExRunnerRuleConfig] {0}: Rules 배열이 비어 있습니다. 게임 모드에 룰이 하나도 없는 상태입니다."),
			FText::FromString(GetName())
		));
	}

	// nullptr 룰 방어 검증
	for (int32 i = 0; i < Rules.Num(); ++i)
	{
		if (!IsValid(Rules[i]))
		{
			Context.AddError(FText::Format(
				NSLOCTEXT("ExDataCenter", "NullRule",
					"[ExRunnerRuleConfig] {0}: Rules[{1}]이 nullptr입니다. 유효한 룰을 지정하세요."),
				FText::FromString(GetName()),
				FText::AsNumber(i)
			));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif
