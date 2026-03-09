// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "GameplayTagContainer.h"
#include "ExRunnerMatchViewModel.generated.h"

class AExGameStateBase;
class UCommonAnimatedSwitcher;

/**
 * GameState의 MatchPhase 변경 사항을 감지하여, 
 * HUD 내 CommonAnimatedSwitcher 위젯에 바인딩할 ActiveWidgetIndex(정수)를 반환하는 브릿지입니다.
 */
UCLASS(BlueprintType)
class EXRUNNERPLAYRUNTIME_API UExRunnerMatchViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/**
	 * 위젯의 OnActivated, 혹은 Construct 등 초기화 시점에 한 번 호출하여
	 * GameState의 OnMatchPhaseChanged 델리게이트와 뷰모델을 바인딩합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ExUI|RunnerMatch", meta = (WorldContext = "WorldContextObject"))
	void AutoInitialize(UObject* WorldContextObject);

	/**
	 * CommonAnimatedSwitcher 레퍼런스를 직접 전달받아 제어권을 획득합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ExUI|RunnerMatch")
	void BindSwitcher(UCommonAnimatedSwitcher* InSwitcher);

	/**
	 * 스위처 객체에서 Active Widget Index로 뷰 바인딩(View Binding) 할 프로퍼티입니다.
	 * 0: 대기 (WaitingForPlayers / Countdown)
	 * 1: 플레이 (Playing)
	 * 2: 결과 (PostMatch)
	 */
	UFUNCTION(BlueprintPure, FieldNotify, Category = "ExUI|RunnerMatch")
	int32 GetActiveWidgetIndex() const;

private:
	/** GameState의 매치 페이즈 변경 시 호출되는 이벤트 리스너 */
	UFUNCTION()
	void OnMatchPhaseUpdated(const FGameplayTag& OldPhase, const FGameplayTag& NewPhase);

	/** 태그 ➡ 스위처 인덱스 변환 로직 */
	void UpdateIndexByPhase(const FGameplayTag& Phase);

	/** 실제 데이터가 갱신되는 Setter */
	void SetActiveWidgetIndex(int32 NewIndex);

	/** 내부 인덱스 데이터 저장소 */
	UPROPERTY(FieldNotify, Setter, Getter)
	int32 ActiveWidgetIndex = 0;

	/** 제어할 스위처 포인터 */
	UPROPERTY(Transient)
	TObjectPtr<UCommonAnimatedSwitcher> BoundSwitcher = nullptr;

	/** 중복 바인딩 및 해제를 명확히 하기 위한 캐싱 포인터 */
	UPROPERTY(Transient)
	TObjectPtr<AExGameStateBase> BoundGameState = nullptr;
};
