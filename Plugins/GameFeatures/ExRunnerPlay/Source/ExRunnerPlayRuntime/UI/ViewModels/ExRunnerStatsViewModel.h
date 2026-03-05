// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/ViewModels/ExPlayerStatsViewModel.h"
#include "ExRunnerStatsViewModel.generated.h"

class UExRunnerStatComponent;

/**
 * ExCore의 ExPlayerStatsViewModel 기능을 확장하여 RunnerPlay 모듈 전용 스탯(Speed, Distance 등)을 바인딩합니다.
 * Zero-Tick 이벤트 옵저버 패턴을 준수합니다.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Ex Runner Stats ViewModel"))
class EXRUNNERPLAYRUNTIME_API UExRunnerStatsViewModel : public UExPlayerStatsViewModel
{
	GENERATED_BODY()

public:
	// -- UI가 관찰할 반환 데이터 (Getter) --

	/** 현재 스피드 UI 반환 */
	UFUNCTION(BlueprintPure, FieldNotify, Category="ExUI|RunnerViewModel")
	float GetCurrentSpeed() const;

	// -- 바인딩 초기화 세팅 --
	
	/**
	 * 실제 관찰 대상 데이터 모델(이 경우 StatComponent)을 전달받아 바인딩합니다.
	 * 위젯 생성 직후 OnInitialized 등에서 주입합니다.
	 */
	UFUNCTION(BlueprintCallable, Category="ExUI|RunnerViewModel")
	void InitializeRunnerBindings(UExRunnerStatComponent* InStatComponent);

private:
	// 상태 감지용 컴포넌트 포인터 (안전 참조)
	TWeakObjectPtr<UExRunnerStatComponent> BoundStatComponent;

	// UI 갱신 기준 캐시 데이터
	UPROPERTY()
	float CachedSpeed = 0.0f;

	/**
	 * Stat Component 내부에서 스피드 변경 Broadcast가 울리면 호출되는 콜백
	 */
	UFUNCTION()
	void OnSpeedUpdated(float NewSpeed);
};
