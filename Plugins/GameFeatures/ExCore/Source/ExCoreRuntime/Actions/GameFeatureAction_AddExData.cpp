// Copyright ExFrameWork. All Rights Reserved.

#include "Actions/GameFeatureAction_AddExData.h"
#include "Data/Base/ExConfigDataAsset.h"
#include "Data/Base/ExDefinitionDataAsset.h"
#include "Data/Base/ExPresetDataAsset.h"
#include "Subsystems/ExDataCenterSubsystem.h"
#include "GameFeatureData.h"
#include "GameFeaturesSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"

// ─────────────────────────────────────────────────
//  내부 헬퍼 함수
// ─────────────────────────────────────────────────


/** GameFeatureData 패키지 이름을 Feature 식별자(등록 키)로 사용 */
static FName GetFeatureName(const UGameFeatureAction* Action)
{
	if (const UGameFeatureData* GFData = Action->GetGameFeatureData())
	{
		return FName(*GFData->GetOutermost()->GetName());
	}
	return NAME_None;
}

// ─────────────────────────────────────────────────
//  GameFeature 활성화 / 비활성화
// ─────────────────────────────────────────────────

void UGameFeatureAction_AddExData::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	Super::OnGameFeatureActivating(Context);

	// 1. 이미 존재하는 GameInstance(WorldContext)들에 대해 즉시 처리
	if (GEngine)
	{
		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			if (Context.ShouldApplyToWorldContext(WorldContext))
			{
				if (UGameInstance* GameInstance = WorldContext.OwningGameInstance)
				{
					AddToDataCenter(GameInstance, Context);
				}
			}
		}
	}

	// 2. 이후 새롭게 생성되는 GameInstance(예: PIE 실행 시) 처리를 위해 Delegate 바인딩
	FDelegateHandle StartHandle = FWorldDelegates::OnStartGameInstance.AddUObject(
		this, &UGameFeatureAction_AddExData::HandleGameInstanceStart, FGameFeatureStateChangeContext(Context));
	
	GameInstanceStartHandles.Add(StartHandle);
}

void UGameFeatureAction_AddExData::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);

	// 바인딩 해제
	for (FDelegateHandle Handle : GameInstanceStartHandles)
	{
		FWorldDelegates::OnStartGameInstance.Remove(Handle);
	}
	GameInstanceStartHandles.Empty();

	// 1. 존재하는 GameInstance들에서 등록 취소
	if (GEngine)
	{
		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			if (Context.ShouldApplyToWorldContext(WorldContext))
			{
				if (UGameInstance* GameInstance = WorldContext.OwningGameInstance)
				{
					RemoveFromDataCenter(GameInstance, Context);
				}
			}
		}
	}
}

// ─────────────────────────────────────────────────
//  DataCenter 관리부
// ─────────────────────────────────────────────────

void UGameFeatureAction_AddExData::HandleGameInstanceStart(UGameInstance* GameInstance, FGameFeatureStateChangeContext ChangeContext)
{
	if (FWorldContext* WorldContext = GameInstance->GetWorldContext())
	{
		if (ChangeContext.ShouldApplyToWorldContext(*WorldContext))
		{
			AddToDataCenter(GameInstance, ChangeContext);
		}
	}
}

void UGameFeatureAction_AddExData::AddToDataCenter(UGameInstance* GameInstance, const FGameFeatureStateChangeContext& Context)
{
	if (!GameInstance) { return; }

	UExDataCenterSubsystem* DataCenter = GameInstance->GetSubsystem<UExDataCenterSubsystem>();
	if (!DataCenter) { return; }

	const FName FeatureName = GetFeatureName(this);
	if (FeatureName.IsNone()) { return; }

	// Config 등록
	if (IsValid(ConfigAsset))
	{
		DataCenter->RegisterConfig(ConfigAsset, FeatureName);
	}

	// Definition 목록 등록
	for (UExDefinitionDataAsset* DefinitionAsset : DefinitionAssets)
	{
		if (IsValid(DefinitionAsset))
		{
			DataCenter->RegisterDefinition(DefinitionAsset, FeatureName);
		}
	}

	// Preset 목록 등록
	for (UExPresetDataAsset* PresetAsset : PresetAssets)
	{
		if (IsValid(PresetAsset))
		{
			DataCenter->RegisterPreset(PresetAsset, FeatureName);
		}
	}
}

void UGameFeatureAction_AddExData::RemoveFromDataCenter(UGameInstance* GameInstance, const FGameFeatureStateChangeContext& Context)
{
	if (!GameInstance) { return; }

	UExDataCenterSubsystem* DataCenter = GameInstance->GetSubsystem<UExDataCenterSubsystem>();
	if (!DataCenter) { return; }

	const FName FeatureName = GetFeatureName(this);
	if (!FeatureName.IsNone())
	{
		DataCenter->UnregisterByFeature(FeatureName);
	}
}
