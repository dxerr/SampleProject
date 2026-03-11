// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputActionValue.h"
#include "ExInputComponentBase.generated.h"

/**
 * 
 */
UCLASS(Abstract, Blueprintable, ClassGroup=(ExInput), meta=(BlueprintSpawnableComponent))
class EXCORERUNTIME_API UExInputComponentBase : public UActorComponent
{
	GENERATED_BODY()

public:
	UExInputComponentBase(const FObjectInitializer& ObjectInitializer);

	// 컨트롤러나 Pawn에서 이 컴포넌트를 호출하여 초기화(SetupPlayerInputComponent 시점)
	UFUNCTION(BlueprintCallable, Category="ExInput")
	virtual void InitializeInputBindings(class UEnhancedInputComponent* EnhancedInputComponent);

	// ============================================
	// 입력 주입 (Input Injection) 유틸리티: UI 등으로부터 들어온 입력을 정규 엔진 루프로 안전하게 밀어넣음
	// ============================================
	
	// Bool (Digital) 액션 주입 (Jump, Slide 등)
	UFUNCTION(BlueprintCallable, Category="ExInput|Injection")
	void InjectInputBoolForAction(const class UInputAction* Action, bool bValue);

	// Float (Axis 1D) 액션 주입 (전진/후진 등)
	UFUNCTION(BlueprintCallable, Category="ExInput|Injection")
	void InjectInputFloatForAction(const class UInputAction* Action, float Value);
	
	// Vector (Axis 2D/3D) 액션 주입 범용 함수
	UFUNCTION(BlueprintCallable, Category="ExInput|Injection")
	void InjectInputVectorForAction(const class UInputAction* Action, FVector Value);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	// UI 등에서 지속적으로 주입할 입력 상태를 담아두는 버퍼 (매 프레임 주입)
	TMap<const class UInputAction*, struct FInputActionValue> InjectedInputStates;
	virtual void BeginPlay() override;

	// 공통 유틸리티: 이 컴포넌트를 소유한 Pawn을 가져오는 안전한 헬퍼
	UFUNCTION(BlueprintPure, Category="ExInput")
	class APawn* GetOwningPawn() const;

	// 공통 유틸리티: 이 컴포넌트를 소유한 (또는 연관된) PlayerController 헬퍼
	UFUNCTION(BlueprintPure, Category="ExInput")
	class APlayerController* GetOwningPlayerController() const;
};
