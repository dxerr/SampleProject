#pragma once

#include "CoreMinimal.h"
#include "ExModalWidget.h"
#include "UI/Data/ExPopupDescriptor.h"
#include "ExPopupWidget.generated.h"

class UCommonRichTextBlock;
class UPanelWidget;
class UEditableTextBox;
class UCommonButtonBase;

// BP 전용: Dynamic Multicast
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnExPopupResultBP,
	EExModalResult, Result,
	const FText&, InputText);

// C++ 전용: Native Delegate
DECLARE_DELEGATE_TwoParams(
	FOnExPopupResultNative,
	EExModalResult /*Result*/,
	const FText& /*InputText*/);

/**
 * Descriptor 기반 동적 팝업 위젯
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class EXCORERUNTIME_API UExPopupWidget : public UExModalWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ExUI|Popup")
	void InitFromDescriptor(const FExPopupDescriptor& Descriptor);

	UPROPERTY(BlueprintAssignable, Category = "ExUI|Popup")
	FOnExPopupResultBP OnPopupResult;

	FOnExPopupResultNative OnPopupResultNative;

	UFUNCTION(BlueprintPure, Category = "ExUI|Popup")
	FText GetInputText() const;

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonRichTextBlock> Text_Title;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonRichTextBlock> Text_Body;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> Panel_Buttons;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> Panel_Input;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> EditBox_Input;

	// 동적으로 생성할 기본 버튼 클래스 지정 (BP에서 WBP_BaseButton 셋팅)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ExUI|Popup")
	TSubclassOf<class UExBaseButtonWidget> PopupButtonClass;

	FTimerHandle AutoCloseTimerHandle;

	UPROPERTY(BlueprintReadOnly, Category = "ExUI|Popup")
	FExPopupDescriptor CurrentDescriptor;

	void SetupButtons(const TArray<FExPopupButtonDesc>& ButtonDescs);
	void SetupInputField(const FExPopupInputDesc& InputDesc);
	void StartAutoCloseTimer(float Seconds);

	UFUNCTION()
	void OnButtonClicked(EExModalResult Result);

	virtual void CloseModalWithResult(EExModalResult Result) override;
};
