// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/ExBaseButtonWidget.h"
#include "CommonTextBlock.h"

void UExBaseButtonWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 에디터 뷰포트에서 Text 값을 수정하면 즉각 반영되도록 처리
	if (ButtonTextBlock)
	{
		ButtonTextBlock->SetText(ButtonText);
	}
}

void UExBaseButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ButtonTextBlock)
	{
		ButtonTextBlock->SetText(ButtonText);
	}

	// 애니메이션 및 스타일 갱신 알림
	BP_UpdateButtonVisuals();
}
