// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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

protected:
	virtual void BeginPlay() override;

	// 공통 유틸리티: 이 컴포넌트를 소유한 Pawn을 가져오는 안전한 헬퍼
	UFUNCTION(BlueprintPure, Category="ExInput")
	class APawn* GetOwningPawn() const;

	// 공통 유틸리티: 이 컴포넌트를 소유한 (또는 연관된) PlayerController 헬퍼
	UFUNCTION(BlueprintPure, Category="ExInput")
	class APlayerController* GetOwningPlayerController() const;
};
