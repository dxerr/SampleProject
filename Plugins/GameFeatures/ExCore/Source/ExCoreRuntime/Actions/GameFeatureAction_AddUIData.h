// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFeatureAction.h"
#include "GameFeatureAction_AddUIData.generated.h"

class UExUIDataAsset;
class UExUIManagerSubsystem;

/**
 * GameFeature가 활성화될 때 UI 레지스트리(UExUIDataAsset)들을
 * UExUIManagerSubsystem에 자동으로 등록해주는 액션입니다.
 */
UCLASS(meta=(DisplayName="Add UI Data"))
class EXCORERUNTIME_API UGameFeatureAction_AddUIData : public UGameFeatureAction
{
	GENERATED_BODY()

public:
	// GameFeature가 켜졌을 때 글로벌 레지스트리에 추가할 데이터 에셋들
	UPROPERTY(EditAnywhere, Category="UI")
	TArray<TSoftObjectPtr<UExUIDataAsset>> UIDataList;

	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;

#if WITH_EDITORONLY_DATA
	virtual void AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData) override;
#endif

private:

};
