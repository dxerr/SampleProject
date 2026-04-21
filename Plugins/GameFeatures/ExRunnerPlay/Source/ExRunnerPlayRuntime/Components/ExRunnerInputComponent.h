// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ExInputComponentBase.h"
#include "InputStrategies/ExRunnerInputMode.h"
#include "ExRunnerInputComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunnerJumpRequested, bool, bIsTriggered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunnerSlideRequested, bool, bIsTriggered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunnerSprintRequested, bool, bIsTriggered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunnerMoveRequested, FVector2D, AxisValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunnerLookRequested, float, AxisValue);
/** AutoRun 모드 전용: 레인 이동 방향 (+1=우, -1=좌) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunnerLaneChangeRequested, int32, LaneDirection);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunnerInputModeChanged, EExRunnerInputMode, NewMode);
class UInputAction;
class UExRunnerInputStrategy;
class UExRunnerConfig;
class UExRunnerMovementComponent;

/**
 * 러너 게임용 특화 입력 라우터 컴포넌트
 */
UCLASS(Blueprintable, ClassGroup=(ExInput), meta=(BlueprintSpawnableComponent))
class EXRUNNERPLAYRUNTIME_API UExRunnerInputComponent : public UExInputComponentBase
{
	GENERATED_BODY()

protected:
	// ============================================
	// 0. Enhanced Input 매핑 데이터 (에디터 디테일 패널에서 할당)
	// ============================================
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ExInput|Actions")
	const UInputAction* JumpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ExInput|Actions")
	const UInputAction* SlideAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ExInput|Actions")
	const UInputAction* SprintAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ExInput|Actions")
	const UInputAction* MoveAction;

	// 자동 바인딩을 위한 네이티브 콜백 (Enhanced Input 시그니처)
	void NativeOnJumpAction(const struct FInputActionValue& Value);
	void NativeOnSlideAction(const struct FInputActionValue& Value);
	void NativeOnSprintAction(const struct FInputActionValue& Value);
	void NativeOnMoveAction(const struct FInputActionValue& Value);
	virtual void BeginPlay() override;
	virtual void InitializeInputBindings(class UEnhancedInputComponent* EnhancedInputComponent) override;

	// ============================================
	// 1. Runner 설정 캐시 (DataCenter 연동)
	// ============================================
	UPROPERTY(Transient)
	TObjectPtr<class UExRunnerConfig> CachedConfig;

	/** DataCenter 업데이트 시 구성 데이터를 다시 로드하기 위한 콜백 */
	UFUNCTION()
	void OnDataCenterUpdated();

	
public:
	// ============================================
	// 2. 이벤트 브로드캐스터 (캐릭터 BP가 이를 구독하여 실제 동작 수행)
	// [NOTE] Strategy 클래스가 직접 Broadcast해야 하므로 모두 public 선언
	// ============================================
	UPROPERTY(BlueprintAssignable, Category="ExInput|Runner|Events")
	FOnRunnerJumpRequested OnJumpRequested;

	UPROPERTY(BlueprintAssignable, Category="ExInput|Runner|Events")
	FOnRunnerSlideRequested OnSlideRequested;

	UPROPERTY(BlueprintAssignable, Category="ExInput|Runner|Events")
	FOnRunnerSprintRequested OnSprintRequested;

	UPROPERTY(BlueprintAssignable, Category="ExInput|Runner|Events")
	FOnRunnerMoveRequested OnMoveRequested;

public:
	// 좌우 방향(Yaw) 회전 요청 — public (MovementComponent AddDynamic 접근 필요)
	UPROPERTY(BlueprintAssignable, Category="ExInput|Runner|Events")
	FOnRunnerLookRequested OnLookRequested;

	/** AutoRun 모드 전용: 레인 이동 방향 (+1=우, -1=좌) */
	UPROPERTY(BlueprintAssignable, Category="ExInput|Runner|Events")
	FOnRunnerLaneChangeRequested OnLaneChangeRequested;

	/** 입력 모드 전환 알림 델리게이트 (UI 동기화 목적) */
	UPROPERTY(BlueprintAssignable, Category="ExInput|Runner|Events")
	FOnRunnerInputModeChanged OnInputModeChanged;

protected:
	/** 현재 활성화된 입력 Strategy 인스턴스 */
	UPROPERTY()
	TObjectPtr<UExRunnerInputStrategy> ActiveStrategy;

public:
	/** GameModeDataSet 대체: RunnerConfig 접근자 */
	UFUNCTION(BlueprintPure, Category="ExInput|Runner|Settings")
	class UExRunnerConfig* GetRunnerConfig() const { return CachedConfig; }
	// ============================================
	// 2. Action Requesters (UI 및 Enhanced Input에서 호출하는 진입점)
	// ============================================
	
	// 점프 요청 (로컬 액션)
	UFUNCTION(BlueprintCallable, Category="ExInput|Runner|Actions")
	virtual void RequestJumpAction(bool bIsTriggered);

	// 슬라이드 요청 (로컬 액션)
	UFUNCTION(BlueprintCallable, Category="ExInput|Runner|Actions")
	virtual void RequestSlideAction(bool bIsTriggered);
	
	// 스프린트 요청 (로컬 액션 - 체크박스 연동형)
	UFUNCTION(BlueprintCallable, Category="ExInput|Runner|Actions")
	virtual void RequestSprintAction(bool bIsTriggered);

	// 이동 요청 (축 입력)
	UFUNCTION(BlueprintCallable, Category="ExInput|Runner|Actions")
	virtual void RequestMoveAction(FVector2D AxisValue);

	// 좌우 회전(Look) 요청 (모바일 델타 입력 등)
	UFUNCTION(BlueprintCallable, Category="ExInput|Runner|Actions")
	virtual void RequestLookAction(float YawAxisValue);

	// 이산(Discrete) 레인 변경 요청 (UI 방향 버튼 등에서 호출)
	UFUNCTION(BlueprintCallable, Category="ExInput|Runner|Actions")
	virtual void RequestLaneChange(int32 LaneDirection);

	/** 입력 모드 전환 (Manual ↔ AutoRun 등). BP에서 상황에 따라 호출 */
	UFUNCTION(BlueprintCallable, Category="ExInput|Runner|Mode")
	void SetInputMode(EExRunnerInputMode NewMode);

	/** 현재 활성 입력 모드 반환 */
	UFUNCTION(BlueprintPure, Category="ExInput|Runner|Mode")
	EExRunnerInputMode GetCurrentInputMode() const { return CurrentInputMode; }

	/** DataSet을 대체하는 Config 캐시 반환 */
	UFUNCTION(BlueprintPure, Category = "ExInput|Runner|Settings")
	UExRunnerConfig* GetCachedConfig() const { return CachedConfig; }

	// MovementComponent가 초기화 완료 시 자신을 등록하는 콜백 함수
	void RegisterMovementComponent(UExRunnerMovementComponent* InMovementComp);

	// DataSet 또는 폴백 기본값에서 스와이프 발동 퍼센트를 반환
	UFUNCTION(BlueprintCallable, Category="ExInput|Runner|Settings")
	float GetSwipeActivationPercentage() const;

	// 현재 슬라이드 입력이 InjectedInputStates에 등록되어 있는지 조회
	UFUNCTION(BlueprintCallable, Category="ExInput|Runner|Settings")
	bool IsSlideInputActive() const;

private:
	/** 런타임 현재 모드 추적용 */
	EExRunnerInputMode CurrentInputMode = EExRunnerInputMode::None;

	/** 등록된 MovementComponent 캐시 */
	UPROPERTY(Transient)
	TObjectPtr<UExRunnerMovementComponent> CachedMovementComp;

	/** Strategy 인스턴스 생성 후 초기화 + MovementComponent 바인딩 수행 */
	void ApplyInputMode(EExRunnerInputMode NewMode);
};
