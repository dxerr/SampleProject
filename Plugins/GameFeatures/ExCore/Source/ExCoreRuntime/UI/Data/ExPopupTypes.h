#pragma once

#include "CoreMinimal.h"
#include "ExPopupTypes.generated.h"

// Note: EExModalResult is technically defined in ExModalWidget.h

/**
 * 팝업창(모달)의 형태와 제공 기능을 정의 (토스트 제외)
 */
UENUM(BlueprintType)
enum class EExPopupType : uint8
{
	Info          UMETA(DisplayName = "정보 (텍스트만)"),
	Acknowledge   UMETA(DisplayName = "확인 (버튼 1개)"),
	Confirm       UMETA(DisplayName = "확인/취소 (버튼 2개)"),
	InputPrompt   UMETA(DisplayName = "텍스트 입력 (에디터박스 + 버튼)")
};
