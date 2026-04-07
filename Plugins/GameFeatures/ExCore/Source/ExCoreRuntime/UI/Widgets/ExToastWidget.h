#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "UI/Data/ExToastDescriptor.h"
#include "UI/Widgets/ExPopupWidget.h" // for FOnExPopupResultBP / Native (which are defined in ExPopupWidget.h or ExModalWidget.h)
#include "ExToastWidget.generated.h"

class UCommonRichTextBlock;
class UProgressBar;

// Native delegate for UUIManagerSubsystem to track closed toasts.
DECLARE_DELEGATE_OneParam(FOnToastClosed, UExToastWidget* /*ClosedToast*/);

/**
 * Toast 전용 위젯
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class EXCORERUNTIME_API UExToastWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// To smoothly interpolate the progress bar instead of using timers
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Toast 초기화 (FExToastDescriptor 기반)
	UFUNCTION(BlueprintCallable, Category = "ExUI|Toast")
	void InitToast(const FExToastDescriptor& Descriptor);

	// 외부에서 프로그레스 수동 제어 (로딩 Toast 등)
	UFUNCTION(BlueprintCallable, Category = "ExUI|Toast")
	void SetProgress(float NormalizedValue);

	// 수동 닫기 요청 (Outro 애니메이션 시작)
	UFUNCTION(BlueprintCallable, Category = "ExUI|Toast")
	void CloseToast(bool bForceImmediate = false);

	// 애니메이션이 끝나고 실제로 위젯을 소멸시키기 위한 종결 함수
	UFUNCTION(BlueprintCallable, Category = "ExUI|Toast")
	void FinishCloseToast();

	// 결과 델리게이트 (만료시 Confirmed, 강제/취소시 Cancelled)
	UPROPERTY(BlueprintAssignable, Category = "ExUI|Toast")
	FOnExPopupResultBP OnToastFinished;

	FOnExPopupResultNative OnToastFinishedNative;

	// 서브시스템 전용: Toast 닫힘 알림 (서브시스템이 바인딩)
	FOnToastClosed OnToastClosed;

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonRichTextBlock> Text_Message;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar_Timer;

	UFUNCTION(BlueprintNativeEvent, Category = "ExUI|Toast", meta = (DisplayName = "Play Intro Animation"))
	void BP_PlayIntroAnimation();

	UFUNCTION(BlueprintNativeEvent, Category = "ExUI|Toast", meta = (DisplayName = "Play Outro Animation"))
	void BP_PlayOutroAnimation();

	float TotalDuration = 0.0f;
	float ElapsedTime = 0.0f;
	bool bAutoCountdown = false;
	bool bIsClosing = false;
};
