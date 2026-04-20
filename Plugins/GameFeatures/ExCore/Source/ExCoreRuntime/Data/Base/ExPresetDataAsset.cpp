// Copyright ExFrameWork. All Rights Reserved.

#include "Base/ExPresetDataAsset.h"
#include "Misc/DataValidation.h"

#if WITH_EDITOR
EDataValidationResult UExPresetDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// [공통 검증 1] PresetTag는 반드시 지정되어야 한다.
	if (!PresetTag.IsValid())
	{
		Context.AddError(FText::Format(
			NSLOCTEXT("ExDataCenter", "EmptyPresetTag",
				"[ExPreset] {0}: PresetTag가 비어 있습니다. DataCenter에서 조회할 수 없습니다."),
			FText::FromString(GetName())
		));
		Result = EDataValidationResult::Invalid;
	}

	// 서브클래스에서 Super::IsDataValid() 호출 후
	// Definition 배열 nullptr 체크, 가중치 합계 검증 등을 추가로 구현한다.

	return Result;
}
#endif
