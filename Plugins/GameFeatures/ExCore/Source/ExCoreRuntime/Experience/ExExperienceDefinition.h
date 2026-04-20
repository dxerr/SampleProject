// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ExExperienceDefinition.generated.h"

class UExHUDLayoutWidget;
class UCommonActivatableWidget;

/**
 * 
 */
UCLASS(BlueprintType, Const)
class EXCORERUNTIME_API UExExperienceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 이 게임 경험을 실행할 때 로드할 메인 맵 파일 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay")
	TSoftObjectPtr<UWorld> MapToLoad;

	// 이 경험(맵)에서 활성화해야 할 UI 레이아웃 베이스 (예: WBP_ExRunnerHUDLayout)
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSoftClassPtr<UExHUDLayoutWidget> DefaultHUDLayout;

	// 이 경험에서 띄워야 할 시스템 기본 위젯 (ExUIManager를 통해 추가할 내역)
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TArray<TSoftClassPtr<UCommonActivatableWidget>> ExtraWidgetsToLoad;
};
