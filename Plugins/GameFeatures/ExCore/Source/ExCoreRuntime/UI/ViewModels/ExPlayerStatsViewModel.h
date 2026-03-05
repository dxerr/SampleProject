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
 * Tick 대신 BlueprintReadOnly, FieldNotify 구조체 방송을 사용합니다.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Ex Player Stats ViewModel"))
class EXCORERUNTIME_API UExPlayerStatsViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()
	
public:
	// -- UI가 관찰할 순수 데이터 (Getter) --

	/** 현재 스코어 반환. PlayerState의 OnRep 델리게이트와 연동됨 */
	UFUNCTION(BlueprintPure, FieldNotify, Category="ExUI|ViewModel")
	float GetCurrentScore() const;

	/** 현재 매치 타이머(초) 등 각종 서버 연동 데이터 반환용 뼈대 */
	UFUNCTION(BlueprintPure, FieldNotify, Category="ExUI|ViewModel")
	float GetMatchTimeRemaining() const;


	// -- 로컬 데이터 바인딩 셋업 (Setter & Delegates) --

	/**
	 * 실제 관찰 대상 데이터 모델(이 경우 컴포넌트나 PlayerState)을 전달받아 바인딩합니다.
	 * 주로 위젯 생성 직후 OnInitialized 등에서 주입합니다.
	 */
	UFUNCTION(BlueprintCallable, Category="ExUI|ViewModel")
	void InitializeBindings(AExPlayerStateBase* InPlayerState);

	// TODO: UExStatComponent (HP/MP) 등에 대한 바인드 함수도 이어서 추가 가능합니다.

private:
	// 현재 주시하고 있는 플레이어의 상태
	TWeakObjectPtr<AExPlayerStateBase> BoundPlayerState;

	// 관찰 대상의 현재 상태(복사본). 이 값이 바뀌고 FieldNotify가 송출될 때 UI가 갱신됩니다.
	UPROPERTY()
	float CachedScore = 0.0f;

	UPROPERTY()
	float CachedMatchTime = 0.0f;

	/**
	 * 대상 PlayerState의 점수 변경 델리게이트가 호출될 때 트리거되는 콜백
	 */
	UFUNCTION()
	void OnPlayerScoreUpdated(float NewScore);
};
