// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "ExPlayerStatsViewModel.generated.h"

class AExPlayerStateBase;
class UExStatComponent;

/**
 * [아키텍처 2.3 데이터 분리]를 준수하기 위해 만들어진 플레이어 스탯용 ViewModel입니다.
 * 이 클래스는 UI와 순수 데이터 사이의 중개자(Observer) 역할을 합니다.
 * Tick 대신 UPROPERTY(FieldNotify, Setter, Getter) 기반 이벤트 방송을 사용합니다.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Ex Player Stats ViewModel"))
class EXCORERUNTIME_API UExPlayerStatsViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	// -- UI가 관찰할 MVVM 바인딩 변수 (FieldNotify 속성 포함) --

	/** 현재 스코어. View Bindings의 Source/Target으로 직접 사용 가능 */
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Getter, meta=(AllowPrivateAccess))
	float CurrentScore = 0.0f;

	/** 현재 매치 남은 시간(초). View Bindings의 Source/Target으로 직접 사용 가능 */
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Getter, meta=(AllowPrivateAccess))
	float MatchTimeRemaining = 0.0f;

public:
	// -- Getter / Setter (UPROPERTY Getter/Setter 지정자에 의해 자동 연결) --

	float GetCurrentScore() const { return CurrentScore; }
	void SetCurrentScore(float NewScore);

	float GetMatchTimeRemaining() const { return MatchTimeRemaining; }
	void SetMatchTimeRemaining(float NewTime);

	// -- 바인딩 초기화 세팅 --

	/**
	 * 실제 관찰 대상 데이터 모델을 전달받아 바인딩합니다.
	 * 주로 위젯 생성 직후 OnInitialized 등에서 주입합니다.
	 */
	UFUNCTION(BlueprintCallable, Category="ExUI|ViewModel")
	void InitializeBindings(AExPlayerStateBase* InPlayerState);

private:
	// 현재 주시하고 있는 플레이어의 상태
	TWeakObjectPtr<AExPlayerStateBase> BoundPlayerState;

	/**
	 * 대상 PlayerState의 점수 변경 델리게이트가 호출될 때 트리거되는 콜백
	 */
	UFUNCTION()
	void OnPlayerScoreUpdated(float NewScore);
};
