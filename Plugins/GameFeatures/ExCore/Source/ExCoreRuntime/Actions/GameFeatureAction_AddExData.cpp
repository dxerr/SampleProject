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
#include "Engine/World.h"

// ─────────────────────────────────────────────────
//  내부 헬퍼
// ─────────────────────────────────────────────────

static FName GetFeatureName(const UGameFeatureAction* Action)
{
	if (const UGameFeatureData* GFData = Action->GetGameFeatureData())
	{
		return FName(*GFData->GetOutermost()->GetName());
	}
	return NAME_None;
}

// ─────────────────────────────────────────────────
//  활성화 / 비활성화
// ─────────────────────────────────────────────────

void UGameFeatureAction_AddExData::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	Super::OnGameFeatureActivating(Context);

	UE_LOG(LogTemp, Warning, TEXT("[AddExData] OnGameFeatureActivating — ConfigAsset=%s"), *GetNameSafe(ConfigAsset));

	// ── 1. 이미 존재하는 WorldContext 즉시 처리 (PIE 등) ──
	int32 AppliedCount = 0;
	if (GEngine)
	{
		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			if (Context.ShouldApplyToWorldContext(WorldContext))
			{
				if (UGameInstance* GameInstance = WorldContext.OwningGameInstance)
				{
					UE_LOG(LogTemp, Warning, TEXT("[AddExData] 즉시 처리: GameInstance=%s"), *GetNameSafe(GameInstance));
					AddToDataCenter(GameInstance, Context);
					AppliedCount++;
				}
			}
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[AddExData] 즉시 처리 완료 — 적용 수: %d"), AppliedCount);

	// ── 2. OnWorldInitializedActors 구독 ──
	// 패키징 빌드에서 FWorldDelegates::OnStartGameInstance 는 GameFeature 활성화 전에
	// 이미 발화하여 Race Condition 이 발생한다.
	// OnWorldInitializedActors 는 각 게임 월드의 Actor 초기화 완료 시 발화하므로
	// DataCenter (UGameInstanceSubsystem) 초기화 이후가 보장된다.
	{
		FDelegateHandle Handle = FWorldDelegates::OnWorldInitializedActors.AddUObject(
			this,
			&UGameFeatureAction_AddExData::HandleWorldInitializedActors,
			FGameFeatureStateChangeContext(Context));
		WorldInitializedHandles.Add(Handle);
	}
}

void UGameFeatureAction_AddExData::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);

	for (FDelegateHandle Handle : WorldInitializedHandles)
	{
		FWorldDelegates::OnWorldInitializedActors.Remove(Handle);
	}
	WorldInitializedHandles.Empty();

	// 존재하는 GameInstance들에서 등록 취소
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
//  이벤트 핸들러
// ─────────────────────────────────────────────────

void UGameFeatureAction_AddExData::HandleWorldInitializedActors(const FActorsInitializedParams& Params, FGameFeatureStateChangeContext ChangeContext)
{
	UWorld* World = Params.World;
	if (!World) { return; }

	// 게임 월드(독립 실행 / PIE)만 처리
	if (World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE)
	{
		return;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance) { return; }

	FWorldContext* WorldContext = GameInstance->GetWorldContext();
	if (!WorldContext) { return; }

	UE_LOG(LogTemp, Warning, TEXT("[AddExData] HandleWorldInitializedActors — World=%s"), *World->GetName());

	if (ChangeContext.ShouldApplyToWorldContext(*WorldContext))
	{
		AddToDataCenter(GameInstance, ChangeContext);
	}
}

// ─────────────────────────────────────────────────
//  DataCenter 등록 / 해제
// ─────────────────────────────────────────────────

void UGameFeatureAction_AddExData::AddToDataCenter(UGameInstance* GameInstance, const FGameFeatureStateChangeContext& Context)
{
	if (!GameInstance) { return; }

	UExDataCenterSubsystem* DataCenter = GameInstance->GetSubsystem<UExDataCenterSubsystem>();
	if (!DataCenter) { return; }

	const FName FeatureName = GetFeatureName(this);
	UE_LOG(LogTemp, Warning, TEXT("[AddExData] AddToDataCenter — FeatureName=%s, ConfigAsset=%s"),
		*FeatureName.ToString(), *GetNameSafe(ConfigAsset));

	if (FeatureName.IsNone()) { return; }

	// Config 등록
	if (IsValid(ConfigAsset))
	{
		UE_LOG(LogTemp, Warning, TEXT("[AddExData] RegisterConfig: %s (%s)"),
			*ConfigAsset->GetName(), *ConfigAsset->GetClass()->GetName());
		DataCenter->RegisterConfig(ConfigAsset, FeatureName);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[AddExData] ConfigAsset이 null! DA가 쿠킹에 포함됐는지 확인하세요."));
	}

	// Definition 등록
	for (UExDefinitionDataAsset* DefinitionAsset : DefinitionAssets)
	{
		if (IsValid(DefinitionAsset))
		{
			DataCenter->RegisterDefinition(DefinitionAsset, FeatureName);
		}
	}

	// Preset 등록
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
