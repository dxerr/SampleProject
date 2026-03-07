// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/ViewModels/ExPlayerStatsViewModel.h"
#include "Player/ExPlayerStateBase.h"

void UExPlayerStatsViewModel::SetCurrentScore(float NewScore)
{
	// 값이 실제로 변경된 경우에만 Broadcast (최적화)
	UE_MVVM_SET_PROPERTY_VALUE(CurrentScore, NewScore);
}

void UExPlayerStatsViewModel::SetMatchTimeRemaining(float NewTime)
{
	UE_MVVM_SET_PROPERTY_VALUE(MatchTimeRemaining, NewTime);
}

void UExPlayerStatsViewModel::InitializeBindings(AExPlayerStateBase* InPlayerState)
{
	if (!InPlayerState)
		return;

	BoundPlayerState = InPlayerState;

	// 1. 초기 통신 지연(JIP) 방어를 위해 현재 값을 먼저 한번 긁어옵니다.
	SetCurrentScore(BoundPlayerState->GetScore());

	// 2. 이후 값이 변경될 때마다 OnPlayerScoreUpdated 를 부르도록 이벤트 구독(Bind)
	// (참고: AExPlayerStateBase에 실제 OnScoreChanged 델리게이트 선언 선행 필요)
	// InPlayerState->OnScoreChanged.AddDynamic(this, &UExPlayerStatsViewModel::OnPlayerScoreUpdated);
}

void UExPlayerStatsViewModel::OnPlayerScoreUpdated(float NewScore)
{
	// SetCurrentScore 내부가 UE_MVVM_SET_PROPERTY_VALUE로 처리되므로
	// 값 설정과 Broadcast가 한 번에 처리됩니다.
	SetCurrentScore(NewScore);
}
