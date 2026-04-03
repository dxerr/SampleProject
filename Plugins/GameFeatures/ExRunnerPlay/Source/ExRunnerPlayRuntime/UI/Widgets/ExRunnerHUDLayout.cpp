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
#if WITH_EDITOR
	// [수정] 에디터 테스트 시 F8(Eject) 뷰포트 마우스 갇힘 버그 방지를 위해 캡처 완화
	return FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CaptureDuringMouseDown, /*bHideCursor=*/ false);
#else
	// 모바일 실제 기기나 패키징 빌드에서는 입력 유실 방지를 위해 영구 캡처 및 커서 숨김 유지
	return FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently, /*bHideCursor=*/ true);
#endif
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
