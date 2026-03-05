// Fill out your copyright notice in the Description page of Project Settings.

#include "ExRunnerStatComponent.h"
#include "Math/UnrealMathUtility.h"

UExRunnerStatComponent::UExRunnerStatComponent()
{
	// 순수 데이터 모델이므로 Tick 갱신 오버헤드를 원천 차단합니다.
	PrimaryComponentTick.bCanEverTick = false;
}

void UExRunnerStatComponent::SetCurrentRunningSpeed(float NewSpeed)
{
	// 미세한 소수점 오차로 인한 무의미한 UI 갱신(Broadcast)을 방지하기 위해 1.0f 단위 변화만 감지
	if (!FMath::IsNearlyEqual(CurrentRunningSpeed, NewSpeed, 1.0f))
	{
		CurrentRunningSpeed = NewSpeed;
		OnRunnerSpeedChanged.Broadcast(CurrentRunningSpeed);
	}
}
