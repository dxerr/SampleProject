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

/**
 * Context에서 DataCenter Subsystem을 추출하는 헬퍼.
 * FGameFeatureStateChangeContext는 GetGameInstance()를 제공하지 않으므로
 * GEngine의 WorldContexts를 순회하여 GameInstance를 획득한다.
 */
static UExDataCenterSubsystem* GetDataCenterFromContext(const FGameFeatureStateChangeContext& Context)
{
	if (!GEngine) { return nullptr; }

	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		// WorldContextHandle이 설정된 경우 해당 World만 대상으로 제한
		if (!Context.ShouldApplyToWorldContext(WorldContext)) { continue; }

		if (UGameInstance* GameInstance = WorldContext.OwningGameInstance)
		{
			return GameInstance->GetSubsystem<UExDataCenterSubsystem>();
		}
	}
	return nullptr;
}

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

	UExDataCenterSubsystem* DataCenter = GetDataCenterFromContext(Context);
	if (!DataCenter)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ExDataCenter] AddExData: DataCenter Subsystem을 찾을 수 없습니다."));
		return;
	}

	const FName FeatureName = GetFeatureName(this);
	if (FeatureName.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ExDataCenter] AddExData: Feature 이름을 획득하지 못했습니다."));
		return;
	}

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

void UGameFeatureAction_AddExData::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);

	UExDataCenterSubsystem* DataCenter = GetDataCenterFromContext(Context);
	if (!DataCenter) { return; }

	const FName FeatureName = GetFeatureName(this);
	if (!FeatureName.IsNone())
	{
		DataCenter->UnregisterByFeature(FeatureName);
	}
}
