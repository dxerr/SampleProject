// Fill out your copyright notice in the Description page of Project Settings.

#include "ExInputComponentBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputAction.h"
#include "Engine/LocalPlayer.h"

UExInputComponentBase::UExInputComponentBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UExInputComponentBase::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (InjectedInputStates.Num() > 0)
	{
		if (APlayerController* PC = GetOwningPlayerController())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			{
				TArray<UInputModifier*> Modifiers;
				TArray<UInputTrigger*> Triggers;
				for (const auto& Pair : InjectedInputStates)
				{
					Subsystem->InjectInputForAction(Pair.Key, Pair.Value, Modifiers, Triggers);
				}
			}
		}
	}
}

void UExInputComponentBase::BeginPlay()
{
	Super::BeginPlay();
}

void UExInputComponentBase::InitializeInputBindings(UEnhancedInputComponent* EnhancedInputComponent)
{
	// Base implementation does nothing, derived classes will override and perform actual bindings.
}

APawn* UExInputComponentBase::GetOwningPawn() const
{
	return Cast<APawn>(GetOwner());
}

APlayerController* UExInputComponentBase::GetOwningPlayerController() const
{
	if (APawn* OwningPawn = GetOwningPawn())
	{
		return Cast<APlayerController>(OwningPawn->GetController());
	}
	
	// 소유자가 폰이 아니라 직접 플레이어 컨트롤러인 경우
	return Cast<APlayerController>(GetOwner());
}

void UExInputComponentBase::InjectInputBoolForAction(const UInputAction* Action, bool bValue)
{
	if (!Action) return;

	if (bValue)
	{
		// [매우 중요] FInputActionValue의 내장 'ValueType'이 Boolean으로 매칭되도록 명시적 생성
		InjectedInputStates.Add(Action, FInputActionValue(true)); 
	}
	else
	{
		InjectedInputStates.Remove(Action); // 프레임 루프에서 제거되어 엔진에서 자동 초기화
	}
}

void UExInputComponentBase::InjectInputFloatForAction(const UInputAction* Action, float Value)
{
	if (!Action) return;

	if (FMath::IsNearlyZero(Value))
	{
		InjectedInputStates.Remove(Action);
	}
	else
	{
		InjectedInputStates.Add(Action, FInputActionValue(Value)); // 명시적인 Axis1D 타입 생성
	}
}

void UExInputComponentBase::InjectInputVectorForAction(const UInputAction* Action, FVector Value)
{
	if (!Action) return;

	if (Value.IsNearlyZero())
	{
		InjectedInputStates.Remove(Action);
	}
	else
	{
		InjectedInputStates.Add(Action, FInputActionValue(Value)); // 명시적인 Axis3D 타입 지향
	}
}
