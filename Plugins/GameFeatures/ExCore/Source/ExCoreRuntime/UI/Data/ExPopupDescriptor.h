#pragma once

#include "CoreMinimal.h"
#include "ExPopupTypes.h"
#include "UI/Widgets/ExModalWidget.h" // For EExModalResult
#include "ExPopupDescriptor.generated.h"

/**
 * 팝업 버튼 설정
 */
USTRUCT(BlueprintType)
struct EXCORERUNTIME_API FExPopupButtonDesc
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Popup|Button")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Popup|Button")
	EExModalResult ResultValue = EExModalResult::Confirm;

	/**
	 * 버튼의 고정 크기 (Width, Height).
	 * X 또는 Y 값이 0.f이면 해당 축은 자동 크기(Content 기반)로 동작합니다.
	 * 기본값(160 x 70)은 모바일 터치 환경에 적합한 표준 크기입니다.
	 * 예: FVector2D(200.f, 80.f) → 가로 200, 세로 80 고정
	 * 예: FVector2D(0.f, 0.f)   → 양쪽 모두 자동 크기
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Popup|Button",
		meta = (DisplayName = "버튼 크기 (0이면 자동)"))
	FVector2D ButtonSize = FVector2D(160.f, 70.f);
};

/**
 * 에디터박스(입력창) 설정
 */
USTRUCT(BlueprintType)
struct EXCORERUNTIME_API FExPopupInputDesc
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Popup|Input")
	FText PlaceholderText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Popup|Input", meta = (ClampMin = "0", ClampMax = "1000"))
	int32 MaxLength = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Popup|Input")
	bool bIsPassword = false;
};

/**
 * 모달형 팝업 전용 Descriptor
 */
USTRUCT(BlueprintType)
struct EXCORERUNTIME_API FExPopupDescriptor
{
	GENERATED_BODY()

	// ─── 팝업 타입 ───
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Popup")
	EExPopupType PopupType = EExPopupType::Info;

	// ─── 공통 필드 ───
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Popup|Content")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Popup|Content")
	FText Body;

	// ─── 버튼 배열 (Acknowledge, Confirm, InputPrompt) ───
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Popup|Buttons",
		meta = (EditCondition = "PopupType == EExPopupType::Acknowledge || PopupType == EExPopupType::Confirm || PopupType == EExPopupType::InputPrompt", EditConditionHides))
	TArray<FExPopupButtonDesc> Buttons;

	// ─── 에디터박스 설정 (InputPrompt) ───
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Popup|Input",
		meta = (EditCondition = "PopupType == EExPopupType::InputPrompt", EditConditionHides))
	FExPopupInputDesc InputConfig;

	// ─── 자동 닫힘 (Info 전용) ───
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Popup|AutoClose",
		meta = (EditCondition = "PopupType == EExPopupType::Info", EditConditionHides))
	float AutoCloseSeconds = 3.0f;

	// ─── Modal 여부 (거의 항상 true 권장) ───
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Popup|Behavior")
	bool bIsModal = true;
};
