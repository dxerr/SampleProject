// Copyright ExFrameWork. All Rights Reserved.

#include "Base/ExDefinitionDataAsset.h"
#include "Misc/DataValidation.h"

#if WITH_EDITOR
EDataValidationResult UExDefinitionDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// [공통 검증 1] DefinitionTag는 반드시 지정되어야 한다.
	if (!DefinitionTag.IsValid())
	{
		Context.AddError(FText::Format(
			NSLOCTEXT("ExDataCenter", "EmptyDefinitionTag",
				"[ExDefinition] {0}: DefinitionTag가 비어 있습니다. DataCenter 검색이 불가능합니다."),
			FText::FromString(GetName())
		));
		Result = EDataValidationResult::Invalid;
	}

	// 서브클래스에서 Super::IsDataValid() 호출 후 필수 레퍼런스 등을 추가로 검증한다.

	return Result;
}
#endif
