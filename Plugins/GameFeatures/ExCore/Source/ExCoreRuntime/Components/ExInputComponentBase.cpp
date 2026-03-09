// Fill out your copyright notice in the Description page of Project Settings.

#include "ExInputComponentBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UExInputComponentBase::UExInputComponentBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
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
