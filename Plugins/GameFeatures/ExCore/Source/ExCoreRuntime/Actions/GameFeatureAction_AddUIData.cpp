// Fill out your copyright notice in the Description page of Project Settings.

#include "Actions/GameFeatureAction_AddUIData.h"
#include "UI/Data/ExUIDataAsset.h"
#include "UI/Subsystems/ExUIManagerSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/AssetManager.h"
#include "GameFeaturesSubsystemSettings.h"

void UGameFeatureAction_AddUIData::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	Super::OnGameFeatureActivating(Context);

	// [Lazy Initialization] 
	// 지연 로딩을 위해 현재 접속한, 혹은 앞으로 접속할 
	// 모든 UExUIManagerSubsystem 공유 대기열에 이 데이터 에셋을 추가합니다.
	for (const TSoftObjectPtr<UExUIDataAsset>& SoftUIData : UIDataList)
	{
		UExUIManagerSubsystem::AddPendingUIData(SoftUIData);
	}
}

void UGameFeatureAction_AddUIData::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);

	for (const TSoftObjectPtr<UExUIDataAsset>& SoftUIData : UIDataList)
	{
		UExUIManagerSubsystem::RemovePendingUIData(SoftUIData);
	}
}

#if WITH_EDITORONLY_DATA
void UGameFeatureAction_AddUIData::AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData)
{
	if (UAssetManager::IsInitialized())
	{
		for (const TSoftObjectPtr<UExUIDataAsset>& SoftUIData : UIDataList)
		{
			AssetBundleData.AddBundleAsset(UGameFeaturesSubsystemSettings::LoadStateClient, SoftUIData.ToSoftObjectPath().GetAssetPath());
			AssetBundleData.AddBundleAsset(UGameFeaturesSubsystemSettings::LoadStateServer, SoftUIData.ToSoftObjectPath().GetAssetPath());
		}
	}
}
#endif
