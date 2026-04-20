// Copyright ExFrameWork. All Rights Reserved.

#include "Base/ExConfigDataAsset.h"
#include "Misc/DataValidation.h"

#if WITH_EDITOR
EDataValidationResult UExConfigDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// 서브클래스에서 Super::IsDataValid() 호출 후 추가 검증을 작성한다.
	// 이 베이스 구현은 공통 검증(추후 확장)의 진입점 역할을 한다.

	return Result;
}
#endif
