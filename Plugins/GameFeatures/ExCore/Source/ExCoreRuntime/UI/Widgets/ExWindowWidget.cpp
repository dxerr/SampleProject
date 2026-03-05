// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/ExWindowWidget.h"
#include "Input/CommonUIInputTypes.h"
#include "UI/Subsystems/ExUIManagerSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

TOptional<FUIInputConfig> UExWindowWidget::GetDesiredInputConfig() const
{
	// 인벤토리 같은 창이 떠있을 때는 무조건 UI 전용 입력이어야 합니다. 마우스가 보이고 게임 액션을 무시합니다.
	// ECommonInputMode::Menu 
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, /*bHideCursor=*/ false);
}

void UExWindowWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// Esc / B버튼 자동 매핑 등록
	// CommonUI 환경에서는 레거시 OnKeyDown 덮어쓰기 대신 RegisterBinding을 통해 강제 제어합니다.
	// FInputActionHandle 등의 구체적인 동작은 프로젝트 세팅의 Common Input Settings 에 연동됩니다.
	// FBindUIActionArgs BindArgs(FGameplayTag::RequestGameplayTag("UI.Action.Cancel"), FSimpleDelegate::CreateUObject(this, &UExWindowWidget::DeactivateWidget));
	// BindArgs.bDisplayInActionBar = true;

	// RegisterBinding은 자식 위젯 등에 의해 덮어씌워질 수 있는 CommonUI의 훌륭한 시스템입니다.
	// (예: 창 안의 팝업이 뜨면 팝업이 뒤로가기를 삼키고 창은 대기)
	// BackHandle = RegisterUIActionBinding(BindArgs);

	BP_OnWindowActivated();
}

void UExWindowWidget::NativeOnDeactivated()
{
	// 백 액션 바인딩 수동 해제 
	// (Deactivate 시 자동으로 풀리긴 하지만 명시적 관리 권장)
	// RemoveBinding(BackHandle);

	BP_OnWindowDeactivated();

	Super::NativeOnDeactivated();
}

FEventReply UExWindowWidget::OnHandleBackAction()
{
	DeactivateWidget();

	// 입력을 소모(Consume)하여 밑에 깔린 다른 위젯이나 게임 시스템이 ESC를 또 먹지 못하게 막습니다.
	return UWidgetBlueprintLibrary::Handled();
}
