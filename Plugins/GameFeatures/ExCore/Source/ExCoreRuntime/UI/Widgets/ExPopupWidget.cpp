#include "UI/Widgets/ExPopupWidget.h"
#include "Components/PanelWidget.h"
#include "Components/EditableTextBox.h"
#include "CommonRichTextBlock.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Components/HorizontalBoxSlot.h"
#include "UI/Widgets/ExBaseButtonWidget.h"
#include "UI/Widgets/ExBaseButtonWidget.h" // We need to assume some button class exists or dynamically add it.
// Assuming ExCore uses UExBaseButtonWidget for auto-generated buttons, but wait, the document says "버튼 동적 생성"
// I will keep it simple and just leave SetupButtons empty indicating BP should implement it, or wait...
// UPanelWidget can add children. Usually button instantiation requires a WidgetClass. Since the class isn't provided in the Descriptor, we might need a property for the generic ButtonClass.
// However, the focus of the request is core architecture. I'll implement what's possible.

// Since there is no ButtonClass specified in the Descriptor or Widget, I will just control visibility of existing buttons if they exist, or emit a warning if it's purely dynamic. Let's add a placeholder comment for SetupButtons.

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
			// OnClicked()는 매개변수가 없는 FCommonButtonEvent를 반환하므로,
			// Payload 기능을 이용해 람다나 추가 인자를 전달할 수 있습니다.
			NewButton->OnClicked().AddUObject(this, &UExPopupWidget::OnButtonClicked, Desc.ResultValue);

			// 패널에 세팅하고 패딩(여백) 살짝 주기
			UPanelSlot* AddedSlot = Panel_Buttons->AddChild(NewButton);
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
