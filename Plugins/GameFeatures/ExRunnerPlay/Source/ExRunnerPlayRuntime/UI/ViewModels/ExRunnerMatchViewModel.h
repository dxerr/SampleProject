// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "GameplayTagContainer.h"
#include "Struct/EExRunnerGameOverReason.h"
#include "ExRunnerMatchViewModel.generated.h"

class AExGameStateBase;
class AExRunnerGameState;
class UCommonAnimatedSwitcher;
class UExUIManagerSubsystem;
class UExRunnerFadeOverlayWidget;

/**
 * GameState의 MatchPhase 변경 사항을 감지하여, 
 * HUD 내 CommonAnimatedSwitcher 위젯에 바인딩할 ActiveWidgetIndex(정수)를 반환하는 브릿지입니다.
 */
UCLASS(BlueprintType, Blueprintable)
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
	 * FallDeath 발생 시 표시할 페이드오버레이 위젯 클래스
	 * GameMode BP의 Details에서 WBP_FadeOverlay(UExRunnerFadeOverlayWidget) 에앤 연결
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Category = "ExUI|RunnerMatch")
	TSubclassOf<UExRunnerFadeOverlayWidget> FadeOverlayWidgetClass;

	/**
	 * 스위처 객체에서 Active Widget Index로 뷰 바인딩(View Binding) 할 프로퍼티입니다.
	 * 0: 대기 (WaitingForPlayers / Countdown)
	 * 1: 플레이 (Playing)
	 * 2: 결과 (PostMatch)
	 */
	UFUNCTION(BlueprintPure, FieldNotify, Category = "ExUI|RunnerMatch")
	int32 GetActiveWidgetIndex() const;

	// ── 룰 시스템 추가 FieldNotify ────────────────────────────────

	/** 타이머 HUD 위젯 갱신용 잔여 시간 (클라이언트 보간 대상) */
	UFUNCTION(BlueprintPure, FieldNotify, Category = "ExUI|RunnerMatch")
	float GetRemainingTime() const;

	/** 타이머 경고 애니메이션 트리거 */
	UFUNCTION(BlueprintPure, FieldNotify, Category = "ExUI|RunnerMatch")
	bool GetIsTimerWarning() const;

	/** 결과 화면 내용 분기용 게임오버 원인 */
	UFUNCTION(BlueprintPure, FieldNotify, Category = "ExUI|RunnerMatch")
	EExRunnerGameOverReason GetGameOverReason() const;

private:
	/** GameState의 매치 페이즈 변경 시 호출되는 이벤트 리스너 */
	UFUNCTION()
	void OnMatchPhaseUpdated(const FGameplayTag& OldPhase, const FGameplayTag& NewPhase);

	/** 태그 ➡ 스위처 인덱스 변환 로직 */
	void UpdateIndexByPhase(const FGameplayTag& Phase);

	/** 실제 데이터가 갱신되는 Setter */
	void SetActiveWidgetIndex(int32 NewIndex);

	// ── 룰 시스템 콜백 & Setter ──────────────────────────────────

	/** GameState::OnRemainingTimeChanged 수신 */
	UFUNCTION()
	void OnRemainingTimeUpdated(int32 NewSeconds);

	/** GameState::OnGameOverReasonChanged 수신 — UI 분기 결정 지점 */
	UFUNCTION()
	void OnGameOverReasonUpdated(EExRunnerGameOverReason Reason);

	/** 개별 플레이어 탈락 시 호출 (FallDeath 등) */
	UFUNCTION()
	void OnLocalPlayerEliminated(EExRunnerGameOverReason Reason);

	void BindLocalPlayerState();

	void SetRemainingTime(float NewTime);
	void SetIsTimerWarning(bool bNewWarning);
	void SetGameOverReasonValue(EExRunnerGameOverReason NewReason);

	// ── 프로퍼티 ──────────────────────────────────────────────────

	/** 내부 인덱스 데이터 저장소 */
	UPROPERTY(FieldNotify, Setter = SetActiveWidgetIndex, Getter)
	int32 ActiveWidgetIndex = 0;

	/** 잔여 시간 (로컬 보간용 float) */
	UPROPERTY(FieldNotify, Setter = SetRemainingTime, Getter)
	float RemainingTime = 0.f;

	/** 타이머 경고 구간 여부 */
	UPROPERTY(FieldNotify, Setter = SetIsTimerWarning, Getter = GetIsTimerWarning)
	bool bIsTimerWarning = false;

	/** 게임오버 원인 */
	UPROPERTY(FieldNotify, Setter = SetGameOverReasonValue, Getter)
	EExRunnerGameOverReason GameOverReason = EExRunnerGameOverReason::None;

	/** 제어할 스위처 포인터 */
	UPROPERTY(Transient)
	TObjectPtr<UCommonAnimatedSwitcher> BoundSwitcher = nullptr;

	/** 중복 바인딩 및 해제를 명확히 하기 위한 캐싱 포인터 */
	UPROPERTY(Transient)
	TObjectPtr<AExGameStateBase> BoundGameState = nullptr;

	/** 룰 바인딩을 위한 AExRunnerGameState 캐싱 포인터 */
	UPROPERTY(Transient)
	TObjectPtr<AExRunnerGameState> BoundRunnerGameState = nullptr;
};
