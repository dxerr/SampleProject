// Fill out your copyright notice in the Description page of Project Settings.

#include "ExRunnerHUDLayout.h"
#include "Components/WidgetSwitcher.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "../Components/ExRunnerInputComponent.h"

TOptional<FUIInputConfig> UExRunnerHUDLayout::GetDesiredInputConfig() const
{
	// 러너 게임 특성상 게임 패드/가상 조이스틱/터치 입력을 온전히 받아야 하므로 Game 입력 모드로 고정합니다.
	// UI 네비게이션은 비활성화되고 폰(Pawn)이 입력을 독점적으로 처리하게 됩니다.
	// [중요] 로비(WBP_LobbyList)도 이 클래스를 상속하므로 마우스 커서를 숨기지 않도록 All+NoCapture를 사용합니다.
	// 실제 Runner 게임플레이 중에는 마우스 무관하게 터치/Enhanced Input으로 처리됩니다.
	return FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::NoCapture, /*bHideCursor=*/ false);
}

void UExRunnerHUDLayout::NativeOnActivated()
{
	Super::NativeOnActivated();
	
	// 바인딩 플래그 초기화
	bIsInputBound = false;
}

void UExRunnerHUDLayout::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 폰(Pawn)이 스폰 & 빙의(Possess)되기 전에는 InputComponent가 없으므로 계속 확인
	if (!bIsInputBound && InputPadSwitcher)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				if (UExRunnerInputComponent* InputComp = Pawn->FindComponentByClass<UExRunnerInputComponent>())
				{
					// 1. 현재 모드 단발성 즉시 동기화 적용
					HandleInputModeChanged(InputComp->GetCurrentInputMode());
					
					// 2. 델리게이트 동적 바인딩 (중복 방지)
					InputComp->OnInputModeChanged.AddUniqueDynamic(this, &UExRunnerHUDLayout::HandleInputModeChanged);
					
					bIsInputBound = true; // 바인딩 성공, 이후 틱에서는 조회 안 함
				}
			}
		}
	}
}

void UExRunnerHUDLayout::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();

	// 바인딩 해제 처리
	if (bIsInputBound && InputPadSwitcher)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				if (UExRunnerInputComponent* InputComp = Pawn->FindComponentByClass<UExRunnerInputComponent>())
				{
					InputComp->OnInputModeChanged.RemoveDynamic(this, &UExRunnerHUDLayout::HandleInputModeChanged);
					bIsInputBound = false;
				}
			}
		}
	}
}

void UExRunnerHUDLayout::HandleInputModeChanged(EExRunnerInputMode NewInputMode)
{
	if (InputPadSwitcher)
	{
		if (NewInputMode == EExRunnerInputMode::AutoButtonRun)
		{
			InputPadSwitcher->SetActiveWidgetIndex(ButtonPadWidgetIndex);
		}
		else
		{
			InputPadSwitcher->SetActiveWidgetIndex(TouchPadWidgetIndex);
		}
	}
}
