// Copyright ExFrameWork. All Rights Reserved.

#include "Subsystems/ExDataCenterSubsystem.h"
#include "Data/Base/ExConfigDataAsset.h"
#include "Data/Base/ExDefinitionDataAsset.h"
#include "Data/Base/ExPresetDataAsset.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY(LogExDataCenter);

// ─────────────────────────────────────────────────
//  초기화 / 해제
// ─────────────────────────────────────────────────

void UExDataCenterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogExDataCenter, Log, TEXT("UExDataCenterSubsystem 초기화 완료."));
}

void UExDataCenterSubsystem::Deinitialize()
{
	ConfigMap.Empty();
	DefinitionMap.Empty();
	PresetMap.Empty();
	FeatureRegistryMap.Empty();
	UE_LOG(LogExDataCenter, Log, TEXT("UExDataCenterSubsystem 해제 완료."));
	Super::Deinitialize();
}

// ─────────────────────────────────────────────────
//  등록
// ─────────────────────────────────────────────────

void UExDataCenterSubsystem::RegisterConfig(UExConfigDataAsset* ConfigAsset, FName FeatureName)
{
	if (!ensureMsgf(IsValid(ConfigAsset), TEXT("[ExDataCenter] RegisterConfig: nullptr ConfigAsset이 전달되었습니다.")))
	{
		return;
	}

	UClass* AssetClass = ConfigAsset->GetClass();

	// [Fix] 블루프린트 상속 구조 지원: 부모 클래스 계층 전부에 대해 등록 (Native 조회 지원용)
	UClass* CurrentClass = AssetClass;
	while (CurrentClass && CurrentClass->IsChildOf(UExConfigDataAsset::StaticClass()))
	{
		if (!ConfigMap.Contains(CurrentClass))
		{
			ConfigMap.Add(CurrentClass, ConfigAsset);
			
			// Feature 추적 등록
			FExRegisteredEntry Entry;
			Entry.Class = CurrentClass;
			Entry.EntryType = FExRegisteredEntry::EType::Config;
			FeatureRegistryMap.FindOrAdd(FeatureName).Add(Entry);
		}

		if (CurrentClass == UExConfigDataAsset::StaticClass()) break;
		CurrentClass = CurrentClass->GetSuperClass();
	}

	UE_LOG(LogExDataCenter, Log, TEXT("[ExDataCenter] Config 등록 완료: %s (Feature: %s)"),
		*AssetClass->GetName(), *FeatureName.ToString());

	OnDataCenterUpdated.Broadcast();
}

void UExDataCenterSubsystem::RegisterDefinition(UExDefinitionDataAsset* DefinitionAsset, FName FeatureName)
{
	if (!ensureMsgf(IsValid(DefinitionAsset), TEXT("[ExDataCenter] RegisterDefinition: nullptr DefinitionAsset이 전달되었습니다.")))
	{
		return;
	}

	UClass* AssetClass = DefinitionAsset->GetClass();
	const FGameplayTag& Tag = DefinitionAsset->DefinitionTag;

	if (!Tag.IsValid())
	{
		UE_LOG(LogExDataCenter, Warning,
			TEXT("[ExDataCenter] RegisterDefinition: %s의 DefinitionTag가 비어 있습니다. 등록을 건너뜁니다."),
			*DefinitionAsset->GetName());
		return;
	}

	// [Fix] 블루프린트 상속 구조 지원: 부모 클래스 계층 전부에 대해 등록 (Native 조회 지원용)
	UClass* CurrentClass = AssetClass;
	while (CurrentClass && CurrentClass->IsChildOf(UExDefinitionDataAsset::StaticClass()))
	{
		TMap<FGameplayTag, TObjectPtr<UExDefinitionDataAsset>>& InnerMap = DefinitionMap.FindOrAdd(CurrentClass);

		// 태그 중복 체크 (동일 클래스 계층 내에서)
		if (!InnerMap.Contains(Tag))
		{
			InnerMap.Add(Tag, DefinitionAsset);

			// Feature 추적 등록
			FExRegisteredEntry Entry;
			Entry.Class = CurrentClass;
			Entry.Tag = Tag;
			Entry.EntryType = FExRegisteredEntry::EType::Definition;
			FeatureRegistryMap.FindOrAdd(FeatureName).Add(Entry);
		}

		if (CurrentClass == UExDefinitionDataAsset::StaticClass()) break;
		CurrentClass = CurrentClass->GetSuperClass();
	}

	UE_LOG(LogExDataCenter, Log, TEXT("[ExDataCenter] Definition 등록 완료: %s | Tag: %s (Feature: %s)"),
		*AssetClass->GetName(), *Tag.ToString(), *FeatureName.ToString());

	OnDataCenterUpdated.Broadcast();
}

void UExDataCenterSubsystem::RegisterPreset(UExPresetDataAsset* PresetAsset, FName FeatureName)
{
	if (!ensureMsgf(IsValid(PresetAsset), TEXT("[ExDataCenter] RegisterPreset: nullptr PresetAsset이 전달되었습니다.")))
	{
		return;
	}

	UClass* AssetClass = PresetAsset->GetClass();
	const FGameplayTag& Tag = PresetAsset->PresetTag;

	if (!Tag.IsValid())
	{
		UE_LOG(LogExDataCenter, Warning,
			TEXT("[ExDataCenter] RegisterPreset: %s의 PresetTag가 비어 있습니다. 등록을 건너뜁니다."),
			*PresetAsset->GetName());
		return;
	}

	TMap<FGameplayTag, TObjectPtr<UExPresetDataAsset>>& InnerMap = PresetMap.FindOrAdd(AssetClass);

	if (InnerMap.Contains(Tag))
	{
		UE_LOG(LogExDataCenter, Warning,
			TEXT("[ExDataCenter] RegisterPreset: %s 타입에 태그 '%s'가 이미 등록되어 있습니다. 무시합니다."),
			*AssetClass->GetName(), *Tag.ToString());
		return;
	}

	InnerMap.Add(Tag, PresetAsset);

	// Feature 추적 등록
	FExRegisteredEntry Entry;
	Entry.Class = AssetClass;
	Entry.Tag = Tag;
	Entry.EntryType = FExRegisteredEntry::EType::Preset;
	FeatureRegistryMap.FindOrAdd(FeatureName).Add(Entry);

	UE_LOG(LogExDataCenter, Log, TEXT("[ExDataCenter] Preset 등록: %s | Tag: %s (Feature: %s)"),
		*AssetClass->GetName(), *Tag.ToString(), *FeatureName.ToString());

	OnDataCenterUpdated.Broadcast();
}

// ─────────────────────────────────────────────────
//  해제
// ─────────────────────────────────────────────────

void UExDataCenterSubsystem::UnregisterByFeature(FName FeatureName)
{
	TArray<FExRegisteredEntry>* Entries = FeatureRegistryMap.Find(FeatureName);
	if (!Entries)
	{
		UE_LOG(LogExDataCenter, Warning,
			TEXT("[ExDataCenter] UnregisterByFeature: Feature '%s'에 등록된 데이터를 찾을 수 없습니다."),
			*FeatureName.ToString());
		return;
	}

	for (const FExRegisteredEntry& Entry : *Entries)
	{
		switch (Entry.EntryType)
		{
		case FExRegisteredEntry::EType::Config:
			ConfigMap.Remove(Entry.Class);
			UE_LOG(LogExDataCenter, Log, TEXT("[ExDataCenter] Config 해제: %s"), *Entry.Class->GetName());
			break;

		case FExRegisteredEntry::EType::Definition:
			if (TMap<FGameplayTag, TObjectPtr<UExDefinitionDataAsset>>* InnerMap = DefinitionMap.Find(Entry.Class))
			{
				InnerMap->Remove(Entry.Tag);
				if (InnerMap->IsEmpty()) { DefinitionMap.Remove(Entry.Class); }
			}
			UE_LOG(LogExDataCenter, Log, TEXT("[ExDataCenter] Definition 해제: %s | Tag: %s"),
				*Entry.Class->GetName(), *Entry.Tag.ToString());
			break;

		case FExRegisteredEntry::EType::Preset:
			if (TMap<FGameplayTag, TObjectPtr<UExPresetDataAsset>>* InnerMap = PresetMap.Find(Entry.Class))
			{
				InnerMap->Remove(Entry.Tag);
				if (InnerMap->IsEmpty()) { PresetMap.Remove(Entry.Class); }
			}
			UE_LOG(LogExDataCenter, Log, TEXT("[ExDataCenter] Preset 해제: %s | Tag: %s"),
				*Entry.Class->GetName(), *Entry.Tag.ToString());
			break;
		}
	}

	FeatureRegistryMap.Remove(FeatureName);
	OnDataCenterUpdated.Broadcast();
	UE_LOG(LogExDataCenter, Log, TEXT("[ExDataCenter] Feature '%s' 데이터 전체 해제 완료."), *FeatureName.ToString());
}

// ─────────────────────────────────────────────────
//  내부 헬퍼
// ─────────────────────────────────────────────────

void UExDataCenterSubsystem::ReportMissingData(const FString& Message) const
{
	ensureMsgf(false, TEXT("%s"), *Message);

#if !UE_BUILD_SHIPPING
	if (!IsRunningDedicatedServer() && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1, 10.f, FColor::Red,
			FString::Printf(TEXT("[ExDataCenter] %s"), *Message));
	}
#endif

	UE_LOG(LogExDataCenter, Error, TEXT("[ExDataCenter] %s"), *Message);
}
