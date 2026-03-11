// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ExInputComponentBase.h"
#include "ExRunnerInputComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunnerJumpRequested, bool, bIsTriggered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunnerSlideRequested, bool, bIsTriggered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunnerSprintRequested, bool, bIsTriggered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunnerMoveRequested, float, AxisValue);

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

public:
	virtual void InitializeInputBindings(class UEnhancedInputComponent* EnhancedInputComponent) override;

	// ============================================
	// 1. 이벤트 브로드캐스터 (캐릭터 BP가 이를 구독하여 실제 동작 수행)
	// ============================================
	UPROPERTY(BlueprintAssignable, Category="ExInput|Runner|Events")
	FOnRunnerJumpRequested OnJumpRequested;

	UPROPERTY(BlueprintAssignable, Category="ExInput|Runner|Events")
	FOnRunnerSlideRequested OnSlideRequested;

	UPROPERTY(BlueprintAssignable, Category="ExInput|Runner|Events")
	FOnRunnerSprintRequested OnSprintRequested;

	UPROPERTY(BlueprintAssignable, Category="ExInput|Runner|Events")
	FOnRunnerMoveRequested OnMoveRequested;

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
	virtual void RequestMoveAction(float AxisValue);
};
