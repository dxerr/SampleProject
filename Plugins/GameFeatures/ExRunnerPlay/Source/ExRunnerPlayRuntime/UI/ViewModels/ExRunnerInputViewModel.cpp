#include "ExRunnerInputViewModel.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "../Components/ExRunnerInputComponent.h"

void UExRunnerInputViewModel::OnJumpButtonPressed()
{
	if (UExRunnerInputComponent* InputComp = GetRunnerInputComponent())
	{
		// 버튼을 누르는 순간 키보드 입력과 동일하게 True (눌림 판정)
		InputComp->RequestJumpAction(true);
	}
}

void UExRunnerInputViewModel::OnJumpButtonReleased()
{
	if (UExRunnerInputComponent* InputComp = GetRunnerInputComponent())
	{
		// 버튼에서 손을 뗄 때 False (뗌 판정 - 체공 시간 등 조절에 유효)
		InputComp->RequestJumpAction(false);
	}
}

void UExRunnerInputViewModel::OnSlideButtonPressed()
{
	if (UExRunnerInputComponent* InputComp = GetRunnerInputComponent())
	{
		InputComp->RequestSlideAction(true);
	}
}

void UExRunnerInputViewModel::OnSlideButtonReleased()
{
	if (UExRunnerInputComponent* InputComp = GetRunnerInputComponent())
	{
		InputComp->RequestSlideAction(false);
	}
}

void UExRunnerInputViewModel::OnSprintCheckStateChanged(bool bIsChecked)
{
	if (UExRunnerInputComponent* InputComp = GetRunnerInputComponent())
	{
		InputComp->RequestSprintAction(bIsChecked);
	}
}

UExRunnerInputComponent* UExRunnerInputViewModel::GetRunnerInputComponent() const
{
	// 에디터/PIE 환경 내에서 유효한 첫 번째 로컬 플레이어를 찾아 폰(Pawn)의 컴포넌트를 가져옴
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				return Pawn->FindComponentByClass<UExRunnerInputComponent>();
			}
		}
	}
	return nullptr;
}
