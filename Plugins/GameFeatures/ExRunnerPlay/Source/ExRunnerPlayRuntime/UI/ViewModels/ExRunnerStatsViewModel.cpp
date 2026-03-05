// Fill out your copyright notice in the Description page of Project Settings.

#include "ExRunnerStatsViewModel.h"
#include "../../Components/ExRunnerStatComponent.h"

float UExRunnerStatsViewModel::GetCurrentSpeed() const
{
	return CachedSpeed;
}

void UExRunnerStatsViewModel::InitializeRunnerBindings(UExRunnerStatComponent* InStatComponent)
{
	if (!InStatComponent)
		return;

	BoundStatComponent = InStatComponent;

	// 1. 초기 통신 지연 방어를 위한 초깃값 선 점유
	CachedSpeed = BoundStatComponent->GetCurrentRunningSpeed();

	// 2. 값 변경 이벤트를 구독(Bind)하여 ViewModel 갱신 연결
	BoundStatComponent->OnRunnerSpeedChanged.AddDynamic(this, &UExRunnerStatsViewModel::OnSpeedUpdated);

	// 3. UI 측으로 "초깃값 들어옴" 방송 (첫 그리기)
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetCurrentSpeed);
}

void UExRunnerStatsViewModel::OnSpeedUpdated(float NewSpeed)
{
	// 이벤트 기반 값 갱신
	CachedSpeed = NewSpeed;

	// 바인딩된 UMG 텍스트 노드에게 재렌더링 강제 명령
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetCurrentSpeed);
}
