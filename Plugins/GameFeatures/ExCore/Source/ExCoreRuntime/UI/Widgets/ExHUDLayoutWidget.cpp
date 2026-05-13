// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/ExHUDLayoutWidget.h"
#include "UI/Widgets/ExWindowWidget.h"
#include "UI/Subsystems/ExUIManagerSubsystem.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

TOptional<FUIInputConfig> UExHUDLayoutWidget::GetDesiredInputConfig() const
{
	// HUD 레이아웃 베이스 기본값: UI와 게임 입력을 모두 허용하고 마우스 커서를 표시합니다.
	// 로비처럼 마우스 클릭이 필요한 경우 이 기본값을 그대로 사용합니다.
	// 게임플레이 전용 HUD(예: ExRunnerHUDLayout)에서는 이 함수를 오버라이드하여
	// ECommonInputMode::Game + 커서 숨김 설정을 재정의합니다.
	return FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::NoCapture, /*bHideCursor=*/ false);
}

void UExHUDLayoutWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
	// 위젯 생성 및 컴포넌트 바인딩 완료 직후, 가장 빠른 시점에 스택을 매니저에 등록합니다.
	RegisterStacksToManager();
}

void UExHUDLayoutWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	
	BP_OnHUDActivated();
}

void UExHUDLayoutWidget::NativeOnDeactivated()
{
	BP_OnHUDDeactivated();

	Super::NativeOnDeactivated();
}

void UExHUDLayoutWidget::RegisterStacksToManager()
{
	if (ULocalPlayer* LP = GetOwningLocalPlayer())
	{
		if (UExUIManagerSubsystem* UIManager = LP->GetSubsystem<UExUIManagerSubsystem>())
		{
			// BindWidget 실패로 null이더라도 UIManager에서 경고를 남김
			UIManager->RegisterStacks(GameStack, MenuStack);

			// 디테일 패널에서 지정한 팝업/토스트 클래스를 UIManager에 자동 등록
			// 설정되지 않은 경우 기존 값 유지 (다른 HUD에서 이미 설정했을 수 있음)
			if (PopupWidgetClass)
			{
				UIManager->SetPopupWidgetClass(PopupWidgetClass);
			}
			if (ToastWidgetClass)
			{
				UIManager->SetToastWidgetClass(ToastWidgetClass);
			}

			// ToastContainer가 배치되어 있으면 자동 등록 (BindWidgetOptional — 없어도 OK)
			if (ToastContainer)
			{
				UIManager->RegisterToastContainer(ToastContainer);
				UE_LOG(LogTemp, Log, TEXT("[ExHUDLayoutWidget] ToastContainer 자동 등록 완료."));
			}
		}
	}
}
