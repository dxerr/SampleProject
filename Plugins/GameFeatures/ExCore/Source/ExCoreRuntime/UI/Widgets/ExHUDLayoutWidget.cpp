// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/ExHUDLayoutWidget.h"
#include "UI/Widgets/ExWindowWidget.h"
#include "UI/Subsystems/ExUIManagerSubsystem.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

TOptional<FUIInputConfig> UExHUDLayoutWidget::GetDesiredInputConfig() const
{
	// HUD 레이아웃 베이스는 화면 근간이므로, 오직 게임 액션 입력만 받고 커서를 자동으로 숨깁니다.
	// ECommonInputMode::Game = 일반 플레이 중 마우스 회전 등 가능.
	return FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently, /*bHideCursor=*/ true);
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
