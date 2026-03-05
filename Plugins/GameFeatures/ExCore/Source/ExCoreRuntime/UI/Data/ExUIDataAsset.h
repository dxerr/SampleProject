// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ExUIDataAsset.generated.h"

class UCommonActivatableWidget;

/**
 * UI가 띄워질 대상 스택(레이어)을 지정하는 열거형
 */
UENUM(BlueprintType)
enum class EExUIStackLayer : uint8
{
	Game UMETA(DisplayName="Game (Overlay HUD)"),
	Menu UMETA(DisplayName="Menu (Interactive Window)"),
	Modal UMETA(DisplayName="Modal (Top-most Popup)")
};

/**
 * 활성화할 타겟 위젯 클래스와 목적지(스택)를 묶는 구조체
 */
USTRUCT(BlueprintType)
struct EXCORERUNTIME_API FExUIEntry
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI")
	TSubclassOf<UCommonActivatableWidget> WidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI")
	EExUIStackLayer StackLayer = EExUIStackLayer::Game;
};

/**
 * GameFeature 별로 전용 UI 매핑 데이터를 가지는 데이터 에셋
 * 런타임에 UExUIManagerSubsystem의 Global Registry로 등록됩니다.
 */
UCLASS(BlueprintType, Const, meta=(DisplayName="Ex UI Data Asset"))
class EXCORERUNTIME_API UExUIDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI", meta=(TitleProperty="WidgetClass"))
	TMap<FGameplayTag, FExUIEntry> UIRegistry;
};
