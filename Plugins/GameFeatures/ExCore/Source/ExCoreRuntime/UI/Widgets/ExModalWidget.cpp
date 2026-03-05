// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/ExModalWidget.h"
#include "Input/CommonUIInputTypes.h"

TOptional<FUIInputConfig> UExModalWidget::GetDesiredInputConfig() const
{
	// 팝업이 활성화된 경우 하단 UI 및 게임 로직 입력을 완벽히 차단하고
	// 커서를 고정합니다. (모달 팝업 상태)
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, /*bHideCursor=*/ false);
}

void UExModalWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	BP_OnModalActivated();
}

void UExModalWidget::NativeOnDeactivated()
{
	BP_OnModalDeactivated();

	Super::NativeOnDeactivated();
}

void UExModalWidget::CloseModalWithResult(EExModalResult Result)
{
	// 결과값을 청취자들에게 뿌립니다.
	OnModalResultDelegate.Broadcast(Result);

	// 스택에서 자신을 비활성화(자연스럽게 꺼지도록 처리)
	DeactivateWidget();
}
