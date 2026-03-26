// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ExInputComponentBase.h"
#include "ExRunnerInputComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunnerJumpRequested, bool, bIsTriggered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunnerSlideRequested, bool, bIsTriggered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunnerSprintRequested, bool, bIsTriggered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunnerMoveRequested, FVector2D, AxisValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunnerLookRequested, float, AxisValue);

class UInputAction;

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
	// 1. Runner 설정 (DA_ExGameModeDataSet 또는 개별 값)
	// ============================================
	// 외부에 정의된 GameModeDataSet (할당 시 이 데이터의 설정값을 우선 사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ExInput|Runner|Settings")
	class UExGameModeDataSet* GameModeDataSet;

	// 조이스틱 값을 실제 회전 델타로 적용할 민감도 스케일 (GameModeDataSet이 없으면 이 값 사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ExInput|Runner|Settings")
	float RunnerLookSensitivity = 0.5f; // 높은 값이 들어올 경우를 대비해 기본 스케일을 작게 설정

	// ============================================
	// 2. 이벤트 브로드캐스터 (캐릭터 BP가 이를 구독하여 실제 동작 수행)
	// ============================================
	UPROPERTY(BlueprintAssignable, Category="ExInput|Runner|Events")
	FOnRunnerJumpRequested OnJumpRequested;

	UPROPERTY(BlueprintAssignable, Category="ExInput|Runner|Events")
	FOnRunnerSlideRequested OnSlideRequested;

	UPROPERTY(BlueprintAssignable, Category="ExInput|Runner|Events")
	FOnRunnerSprintRequested OnSprintRequested;

	UPROPERTY(BlueprintAssignable, Category="ExInput|Runner|Events")
	FOnRunnerMoveRequested OnMoveRequested;

	// 좌우 방향(Yaw) 회전 요청 브로드캐스트용 (모바일 터치 패드 등에서 호출)
	// public으로 선언해야 외부 클래스(MovementComponent 등)에서 AddDynamic 접근 가능
public:
	UPROPERTY(BlueprintAssignable, Category="ExInput|Runner|Events")
	FOnRunnerLookRequested OnLookRequested;

protected:

public:
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

	// DataSet 또는 폴백 기본값에서 스와이프 발동 퍼센트를 반환
	// GameModeDataSet이 할당된 경우 DataSet의 SwipeActivationPercentage 우선 사용
	UFUNCTION(BlueprintCallable, Category="ExInput|Runner|Settings")
	float GetSwipeActivationPercentage() const;

	// 현재 슬라이드 입력이 InjectedInputStates에 등록(true 주입 중)되어 있는지 조회
	// ViewModel의 로컬 변수 대신, 컴포넌트 내부 맵을 직접 확인하여 동기화 깨짐 최소화
	UFUNCTION(BlueprintCallable, Category="ExInput|Runner|Settings")
	bool IsSlideInputActive() const;
};
