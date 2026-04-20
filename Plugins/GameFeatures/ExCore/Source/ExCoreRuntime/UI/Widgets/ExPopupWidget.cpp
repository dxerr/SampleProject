#include "UI/Widgets/ExPopupWidget.h"
#include "Components/PanelWidget.h"
#include "Components/EditableTextBox.h"
#include "Components/SizeBox.h"
#include "CommonRichTextBlock.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Components/HorizontalBoxSlot.h"
#include "UI/Widgets/ExBaseButtonWidget.h"

void UExPopupWidget::InitFromDescriptor(const FExPopupDescriptor& Descriptor)
{
	CurrentDescriptor = Descriptor;

	if (Text_Title)
	{
		Text_Title->SetText(Descriptor.Title);
	}

	if (Text_Body)
	{
		Text_Body->SetText(Descriptor.Body);
	}

	// Visibility control based on PopupType
	if (Descriptor.PopupType == EExPopupType::Info)
	{
		if (Panel_Buttons) Panel_Buttons->SetVisibility(ESlateVisibility::Collapsed);
		if (Panel_Input) Panel_Input->SetVisibility(ESlateVisibility::Collapsed);

		if (Descriptor.AutoCloseSeconds > 0.0f)
		{
			StartAutoCloseTimer(Descriptor.AutoCloseSeconds);
		}
	}
	else if (Descriptor.PopupType == EExPopupType::Acknowledge || Descriptor.PopupType == EExPopupType::Confirm)
	{
		if (Panel_Buttons) Panel_Buttons->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (Panel_Input) Panel_Input->SetVisibility(ESlateVisibility::Collapsed);

		SetupButtons(Descriptor.Buttons);
	}
	else if (Descriptor.PopupType == EExPopupType::InputPrompt)
	{
		if (Panel_Buttons) Panel_Buttons->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (Panel_Input) Panel_Input->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		SetupButtons(Descriptor.Buttons);
		SetupInputField(Descriptor.InputConfig);
	}
}

FText UExPopupWidget::GetInputText() const
{
	if (EditBox_Input)
	{
		return EditBox_Input->GetText();
	}
	return FText::GetEmpty();
}

void UExPopupWidget::SetupButtons(const TArray<FExPopupButtonDesc>& ButtonDescs)
{
	if (!Panel_Buttons) return;

	Panel_Buttons->ClearChildren();

	if (!PopupButtonClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ExPopupWidget] PopupButtonClass가 설정되지 않아 버튼을 생성할 수 없습니다!"));
		return;
	}

	for (const FExPopupButtonDesc& Desc : ButtonDescs)
	{
		if (UExBaseButtonWidget* NewButton = CreateWidget<UExBaseButtonWidget>(this, PopupButtonClass))
		{
			// 버튼의 텍스트 지정
			NewButton->ButtonText = Desc.Label;

			// 클릭 이벤트 바인딩 (Native C++ Event)
			NewButton->OnClicked().AddUObject(this, &UExPopupWidget::OnButtonClicked, Desc.ResultValue);

			// ButtonSize가 지정된 경우 USizeBox로 래핑하여 크기 강제 적용
			const bool bHasWidthOverride  = Desc.ButtonSize.X > 0.f;
			const bool bHasHeightOverride = Desc.ButtonSize.Y > 0.f;

			UWidget* ChildToAdd = NewButton;
			if (bHasWidthOverride || bHasHeightOverride)
			{
				USizeBox* SizeBox = NewObject<USizeBox>(this);
				if (bHasWidthOverride)  SizeBox->SetWidthOverride(Desc.ButtonSize.X);
				if (bHasHeightOverride) SizeBox->SetHeightOverride(Desc.ButtonSize.Y);
				SizeBox->AddChild(NewButton);
				ChildToAdd = SizeBox;
			}

			// 패널에 추가하고 HorizontalBoxSlot 패딩 적용
			UPanelSlot* AddedSlot = Panel_Buttons->AddChild(ChildToAdd);
			if (UHorizontalBoxSlot* HBoxSlot = Cast<UHorizontalBoxSlot>(AddedSlot))
			{
				HBoxSlot->SetPadding(FMargin(10.0f, 0.0f, 10.0f, 0.0f));
			}
		}
	}
}

void UExPopupWidget::SetupInputField(const FExPopupInputDesc& InputDesc)
{
	if (EditBox_Input)
	{
		EditBox_Input->SetHintText(InputDesc.PlaceholderText);
		EditBox_Input->SetIsPassword(InputDesc.bIsPassword);
		// MaxLength limit logic usually requires a text changed delegate, or setting the property if UEditableTextBox exposes it.
		// UEditableTextBox has SetIsPassword and SetHintText.
	}
}

void UExPopupWidget::StartAutoCloseTimer(float Seconds)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(AutoCloseTimerHandle, FTimerDelegate::CreateUObject(this, &UExPopupWidget::CloseModalWithResult, EExModalResult::Confirm), Seconds, false);
	}
}

void UExPopupWidget::OnButtonClicked(EExModalResult Result)
{
	CloseModalWithResult(Result);
}

void UExPopupWidget::CloseModalWithResult(EExModalResult Result)
{
	if (AutoCloseTimerHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(AutoCloseTimerHandle);
		}
	}

	FText InputText = GetInputText();

	// 1) BP Delegate Broadcast
	OnPopupResult.Broadcast(Result, InputText);

	// 2) C++ Native Delegate Execute
	OnPopupResultNative.ExecuteIfBound(Result, InputText);

	// 3) Parent (Broadcasts OnModalResultDelegate + Deactivates Widget)
	Super::CloseModalWithResult(Result);
}
