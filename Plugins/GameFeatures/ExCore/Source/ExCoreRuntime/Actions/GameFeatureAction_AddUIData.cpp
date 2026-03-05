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

	// 동기 로딩 방식 (추후 비동기로 고도화 가능)
	RegisterToAllLocalPlayers();
}

void UGameFeatureAction_AddUIData::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);

	UnregisterFromAllLocalPlayers();
}

void UGameFeatureAction_AddUIData::RegisterToAllLocalPlayers()
{
	if (!GEngine) return;

	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		UWorld* World = WorldContext.World();
		if (World && World->IsGameWorld())
		{
			if (UGameInstance* GameInstance = World->GetGameInstance())
			{
				for (ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers())
				{
					if (UExUIManagerSubsystem* UIManager = LocalPlayer->GetSubsystem<UExUIManagerSubsystem>())
					{
						for (const TSoftObjectPtr<UExUIDataAsset>& SoftUIData : UIDataList)
						{
							if (UExUIDataAsset* UIData = SoftUIData.LoadSynchronous())
							{
								UIManager->RegisterUIData(UIData);
							}
						}
					}
				}
			}
		}
	}
}

void UGameFeatureAction_AddUIData::UnregisterFromAllLocalPlayers()
{
	if (!GEngine) return;

	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		UWorld* World = WorldContext.World();
		if (World && World->IsGameWorld())
		{
			if (UGameInstance* GameInstance = World->GetGameInstance())
			{
				for (ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers())
				{
					if (UExUIManagerSubsystem* UIManager = LocalPlayer->GetSubsystem<UExUIManagerSubsystem>())
					{
						for (const TSoftObjectPtr<UExUIDataAsset>& SoftUIData : UIDataList)
						{
							// 이미 메모리에 로드되어 있거나 캐시된 경우에만 지웁니다
							if (UExUIDataAsset* UIData = SoftUIData.Get())
							{
								UIManager->UnregisterUIData(UIData);
							}
						}
					}
				}
			}
		}
	}
}

#if WITH_EDITORONLY_DATA
void UGameFeatureAction_AddUIData::AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData)
{
	if (UAssetManager::IsInitialized())
	{
		for (const TSoftObjectPtr<UExUIDataAsset>& SoftUIData : UIDataList)
		{
			AssetBundleData.AddBundleAsset(UGameFeaturesSubsystemSettings::LoadStateClient, SoftUIData.ToSoftObjectPath());
			AssetBundleData.AddBundleAsset(UGameFeaturesSubsystemSettings::LoadStateServer, SoftUIData.ToSoftObjectPath());
		}
	}
}
#endif
