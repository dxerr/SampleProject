#include "UI/Widgets/ExToastWidget.h"
#include "CommonRichTextBlock.h"
#include "Components/ProgressBar.h"

void UExToastWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bIsClosing)
	{
		return;
	}

	if (bAutoCountdown && TotalDuration > 0.0f)
	{
		ElapsedTime += InDeltaTime;

		if (ProgressBar_Timer)
		{
			float Percent = FMath::Clamp(1.0f - (ElapsedTime / TotalDuration), 0.0f, 1.0f);
			ProgressBar_Timer->SetPercent(Percent);
		}

		if (ElapsedTime >= TotalDuration)
		{
			CloseToast(false); // Natural expiration
		}
	}
}

void UExToastWidget::InitToast(const FExToastDescriptor& Descriptor)
{
	if (Text_Message)
	{
		Text_Message->SetText(Descriptor.Message);
	}

	TotalDuration = Descriptor.Duration;
	ElapsedTime = 0.0f;
	bAutoCountdown = Descriptor.ProgressConfig.bAutoCountdown;
	bIsClosing = false;

	if (ProgressBar_Timer)
	{
		ProgressBar_Timer->SetVisibility(Descriptor.ProgressConfig.bShowProgressBar ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		ProgressBar_Timer->SetPercent(1.0f);
	}

	BP_PlayIntroAnimation();
}

void UExToastWidget::SetProgress(float NormalizedValue)
{
	if (ProgressBar_Timer && !bAutoCountdown && !bIsClosing)
	{
		ProgressBar_Timer->SetPercent(FMath::Clamp(NormalizedValue, 0.0f, 1.0f));
	}
}

void UExToastWidget::CloseToast(bool bForceImmediate)
{
	if (bIsClosing) return;
	bIsClosing = true;

	if (bForceImmediate)
	{
		FinishCloseToast();
	}
	else
	{
		// Let BP handle the Outro anim. 
		// The BP should call FinishCloseToast() when the animation is done.
		BP_PlayOutroAnimation();
	}
}

void UExToastWidget::FinishCloseToast()
{
	// Notify the subsystem to update arrays (e.g. pop from Pending Queue)
	OnToastClosed.ExecuteIfBound(this);

	const EExModalResult Result = (ElapsedTime >= TotalDuration && bAutoCountdown)
		? EExModalResult::Confirm
		: EExModalResult::Cancel;

	OnToastFinished.Broadcast(Result, FText::GetEmpty());
	OnToastFinishedNative.ExecuteIfBound(Result, FText::GetEmpty());

	RemoveFromParent();
}

void UExToastWidget::BP_PlayIntroAnimation_Implementation()
{
	// 기본 구현: 아무것도 하지 않음 (BP에서 오버라이드하여 시각적 연출 추가)
}

void UExToastWidget::BP_PlayOutroAnimation_Implementation()
{
	// 주인님의 지적대로, BP에서 애니메이션 노드를 오버라이드 하지 않은 경우 
	// Toast가 화면 잔존 상태로 영원히 파괴되지 않는 버그를 막기 위한 C++ 기본 구현입니다!
	FinishCloseToast();
}
