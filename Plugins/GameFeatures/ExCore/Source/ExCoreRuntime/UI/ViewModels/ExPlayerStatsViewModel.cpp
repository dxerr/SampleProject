// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/ViewModels/ExPlayerStatsViewModel.h"
#include "Player/ExPlayerStateBase.h"
// #include "GameModes/ExGameStateBase.h" 

float UExPlayerStatsViewModel::GetCurrentScore() const
{
	return CachedScore;
}

float UExPlayerStatsViewModel::GetMatchTimeRemaining() const
{
	return CachedMatchTime;
}

void UExPlayerStatsViewModel::InitializeBindings(AExPlayerStateBase* InPlayerState)
{
	if (!InPlayerState)
		return;

	BoundPlayerState = InPlayerState;

	// 1. 초기 통신 지연(JIP) 방어를 위해 현재 값을 먼저 한번 긁어옵니다. (JIP 무결성 규칙 2.3)
	CachedScore = BoundPlayerState->GetScore();
	// 매치 타이머 역시 GameState 획득 후 초기 긁어오기
	
	// 2. 이후 값이 변경될 때마다 OnPlayerScoreUpdated 를 부르도록 이벤트 구독(Bind)
	// (참고: AExPlayerStateBase에 실제 OnScoreChanged 델리게이트 선언 선행 필요)
	// InPlayerState->OnScoreChanged.AddDynamic(this, &UExPlayerStatsViewModel::OnPlayerScoreUpdated);

	// 3. UI 쪽으로 "데이터가 하나 들어왔으니 초깃값을 그려라" 통보
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetCurrentScore);
}

void UExPlayerStatsViewModel::OnPlayerScoreUpdated(float NewScore)
{
	// 1. 값 캐싱
	CachedScore = NewScore;

	// 2. BlueprintReadOnly, FieldNotify로 선언된 프로퍼티를 "바뀌었다"고 허공에 외칩니다(Broadcast).
	// 그러면 이 ViewModel에 귀를 기울이고 있던(바인딩된) UMG Widget이 알아서 자동으로 글씨를 갱신합니다.
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetCurrentScore);
}
