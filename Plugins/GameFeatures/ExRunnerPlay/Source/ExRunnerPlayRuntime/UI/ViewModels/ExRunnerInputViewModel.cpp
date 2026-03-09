#include "ExRunnerInputViewModel.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "../Components/ExRunnerInputComponent.h"

void UExRunnerInputViewModel::OnJumpButtonClicked()
{
	if (UExRunnerInputComponent* InputComp = GetRunnerInputComponent())
	{
		// UI 터치/클릭은 단발성 이벤트이므로 'Started'(false) 성격으로 보냅니다.
		// 블루프린트 기존 로직에서 'Started' 분기에 타야 일반 점프 및 일어서기가 발생합니다.
		InputComp->RequestJumpAction(false);
	}
}

void UExRunnerInputViewModel::OnSlideButtonClicked()
{
	if (UExRunnerInputComponent* InputComp = GetRunnerInputComponent())
	{
		// 슬라이드 클릭도 단발성(Started) 액션으로 보냅니다.
		InputComp->RequestSlideAction(false);
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
