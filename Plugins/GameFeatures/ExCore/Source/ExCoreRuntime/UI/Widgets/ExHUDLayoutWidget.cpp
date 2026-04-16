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
			// 만약 하나라도 BindWidget 실패로 null이라면 매니저에서 경고를 냅니다.
			UIManager->RegisterStacks(GameStack, MenuStack);
		}
	}
}
