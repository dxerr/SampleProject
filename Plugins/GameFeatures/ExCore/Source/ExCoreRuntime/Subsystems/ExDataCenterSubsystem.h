// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "ExDataCenterSubsystem.generated.h"

class UExConfigDataAsset;
class UExDefinitionDataAsset;
class UExPresetDataAsset;

DECLARE_LOG_CATEGORY_EXTERN(LogExDataCenter, Log, All);

/**
 * DataCenter가 갱신되었을 때 브로드캐스트하는 델리게이트.
 * 캐싱 포인터를 보유한 시스템이 구독하여 재조회/무효화 처리를 수행한다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDataCenterUpdated);

/**
 * UExDataCenterSubsystem — 통합 데이터 센터 서브시스템.
 *
 * 역할:
 *   Config / Definition / Preset DataAsset을 중앙에서 등록·조회·해제하는 단일 창구이다.
 *   GameInstance 수명 동안 유지되어 레벨 전환 시에도 데이터 재로딩이 불필요하다.
 *
 * 핵심 원칙:
 *   - 읽기 전용 정적 데이터 전용. 런타임 동적 상태(점수, 세션 등)는 다루지 않는다 (SRP).
 *   - ExCore는 Feature를 참조하지 않는다. Feature가 활성화 시 데이터를 Push한다.
 *
 * 저장소 구조:
 *   - ConfigMap:     TMap<UClass*, UExConfigDataAsset*>                  — 타입 키, 1:1
 *   - DefinitionMap: TMap<UClass*, TMap<FGameplayTag, UExDefinitionDataAsset*>> — 복합키, 태그 유일성 강제
 *   - PresetMap:     TMap<UClass*, TMap<FGameplayTag, UExPresetDataAsset*>>     — 복합키, 타입별 분리
 *
 * 캐싱 규칙 (필수):
 *   조회 결과를 로컬에 보관할 때는 반드시 TWeakObjectPtr<T>를 사용한다.
 *   Raw Pointer 캐싱은 GameFeature 비활성화 시 Stale 참조를 만들 수 있으므로 금지한다.
 *   DataCenter API 호출은 BeginPlay, OnExperienceLoaded 등 초기화 시점에 1회만 수행한다.
 *   Tick/Update 내에서 직접 호출하지 않는다.
 *
 * 에러 처리:
 *   등록되지 않은 타입 접근 시 ensureMsgf + 화면 디버그 메시지(Shipping 빌드 제외)로 즉시 알림.
 *   UE_LOG(Fatal) 사용 금지 — 에디터를 크래시시키지 않는다.
 *
 * 접근 예시:
 *   DataCenter->GetConfig<UExRunnerConfigAsset>()->Curve.FixedCurveRadius
 *   DataCenter->FindDefinition<UExItemDefinition>(Ex.Item.Coin 태그)
 *   DataCenter->GetAllDefinitions<UExObstacleDefinition>()
 *   DataCenter->GetPreset<UExRunnerRulePreset>(Ex.Mode.Endless 태그)
 *
 * 확장 방침:
 *   DA 50개 이상 또는 비동기 로딩이 필수가 되면 내부 저장소를 UAssetManager 기반으로 전환한다.
 *   이때 외부 API(GetConfig, FindDefinition 등) 시그니처는 유지한다.
 */
UCLASS()
class EXCORERUNTIME_API UExDataCenterSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	// ─────────────────────────────────────────────────
	//  등록 / 해제 API (GameFeatureAction_AddExData에서 호출)
	// ─────────────────────────────────────────────────

	/**
	 * Config DA를 등록한다. 클래스당 1개만 허용된다.
	 * @param ConfigAsset 등록할 Config DataAsset
	 * @param FeatureName 비활성화 시 일괄 해제를 위한 Feature 식별자
	 */
	UFUNCTION(BlueprintCallable, Category = "Ex|DataCenter")
	void RegisterConfig(UExConfigDataAsset* ConfigAsset, FName FeatureName);

	/**
	 * Definition DA를 개별 등록한다.
	 * 동일 타입·태그 조합이 이미 있으면 경고 후 무시한다.
	 * @param DefinitionAsset 등록할 Definition DataAsset
	 * @param FeatureName 비활성화 시 일괄 해제를 위한 Feature 식별자
	 */
	UFUNCTION(BlueprintCallable, Category = "Ex|DataCenter")
	void RegisterDefinition(UExDefinitionDataAsset* DefinitionAsset, FName FeatureName);

	/**
	 * Preset DA를 등록한다. 동일 타입·태그 조합이 이미 있으면 경고 후 무시한다.
	 * @param PresetAsset 등록할 Preset DataAsset
	 * @param FeatureName 비활성화 시 일괄 해제를 위한 Feature 식별자
	 */
	UFUNCTION(BlueprintCallable, Category = "Ex|DataCenter")
	void RegisterPreset(UExPresetDataAsset* PresetAsset, FName FeatureName);

	/**
	 * 특정 GameFeature가 등록한 모든 데이터를 일괄 해제한다.
	 * GameFeature 비활성화 시 GameFeatureAction_AddExData에서 호출한다.
	 * @param FeatureName 해제할 Feature 식별자
	 */
	UFUNCTION(BlueprintCallable, Category = "Ex|DataCenter")
	void UnregisterByFeature(FName FeatureName);

	// ─────────────────────────────────────────────────
	//  조회 API — 안정 인터페이스 (시그니처 불변)
	// ─────────────────────────────────────────────────

	/**
	 * 타입을 키로 Config DA를 반환한다.
	 * 미등록 시 ensureMsgf 경고 + 화면 알림 후 nullptr 반환.
	 *
	 * 사용 예:
	 *   UExRunnerConfigAsset* Config = DataCenter->GetConfig<UExRunnerConfigAsset>();
	 *
	 * 호출 시점: BeginPlay 또는 OnExperienceLoaded 등 초기화 시점에 1회만 호출하고 결과를 TWeakObjectPtr로 캐싱한다.
	 */
	template <typename T>
	T* GetConfig() const
	{
		static_assert(TIsDerivedFrom<T, UExConfigDataAsset>::IsDerived,
			"GetConfig<T>: T는 UExConfigDataAsset의 서브클래스여야 합니다.");

		if (const auto* Found = ConfigMap.Find(T::StaticClass()))
		{
			return Cast<T>(*Found);
		}

		// [Debug] 검색 실패 시 맵 상태 출력
		TArray<TObjectPtr<UClass>> Keys;
		ConfigMap.GetKeys(Keys);
		FString KeyList;
		for(TObjectPtr<UClass> K : Keys) { KeyList += K->GetName() + TEXT(", "); }

		ReportMissingData(FString::Printf(TEXT("GetConfig 상세: Target=[%s], MapKeys=[%s]"),
			*T::StaticClass()->GetName(), *KeyList));
		
		return nullptr;
	}

	/**
	 * 태그를 키로 단건 Definition DA를 반환한다.
	 * 미등록 시 nullptr 반환 (경고 없음 — Definition은 Optional 케이스 허용).
	 *
	 * 사용 예:
	 *   UExItemDefinition* Item = DataCenter->FindDefinition<UExItemDefinition>(Ex.Item.Coin 태그);
	 */
	template <typename T>
	T* FindDefinition(const FGameplayTag& Tag) const
	{
		static_assert(TIsDerivedFrom<T, UExDefinitionDataAsset>::IsDerived,
			"FindDefinition<T>: T는 UExDefinitionDataAsset의 서브클래스여야 합니다.");

		if (const auto* InnerMap = DefinitionMap.Find(T::StaticClass()))
		{
			if (const TObjectPtr<UExDefinitionDataAsset>* Found = InnerMap->Find(Tag))
			{
				return Cast<T>(*Found);
			}
		}
		return nullptr;
	}

	/**
	 * 특정 타입의 모든 Definition DA를 TArray로 반환한다.
	 *
	 * 사용 예:
	 *   TArray<UExObstacleDefinition*> Obstacles = DataCenter->GetAllDefinitions<UExObstacleDefinition>();
	 */
	template <typename T>
	TArray<T*> GetAllDefinitions() const
	{
		static_assert(TIsDerivedFrom<T, UExDefinitionDataAsset>::IsDerived,
			"GetAllDefinitions<T>: T는 UExDefinitionDataAsset의 서브클래스여야 합니다.");

		TArray<T*> Result;
		if (const auto* InnerMap = DefinitionMap.Find(T::StaticClass()))
		{
			for (const auto& Pair : *InnerMap)
			{
				if (T* Typed = Cast<T>(Pair.Value))
				{
					Result.Add(Typed);
				}
			}
		}
		return Result;
	}

	/**
	 * 타입 + 모드 태그를 복합키로 Preset DA를 반환한다.
	 * 미등록 시 ensureMsgf 경고 + 화면 알림 후 nullptr 반환.
	 *
	 * 사용 예:
	 *   UExRunnerRulePreset* Preset = DataCenter->GetPreset<UExRunnerRulePreset>(Ex.Mode.Endless 태그);
	 *
	 * 동일 태그를 서로 다른 타입(RulePreset, SpawnPreset)이 공유해도 타입별로 맵이 분리되어 있어 충돌하지 않는다.
	 */
	template <typename T>
	T* GetPreset(const FGameplayTag& ModeTag) const
	{
		static_assert(TIsDerivedFrom<T, UExPresetDataAsset>::IsDerived,
			"GetPreset<T>: T는 UExPresetDataAsset의 서브클래스여야 합니다.");

		if (const auto* InnerMap = PresetMap.Find(T::StaticClass()))
		{
			if (const TObjectPtr<UExPresetDataAsset>* Found = InnerMap->Find(ModeTag))
			{
				return Cast<T>(*Found);
			}
		}

		ReportMissingData(FString::Printf(TEXT("GetPreset: %s 타입의 Preset(태그: %s)이 DataCenter에 없습니다."),
			*T::StaticClass()->GetName(), *ModeTag.ToString()));
		return nullptr;
	}

	// ─────────────────────────────────────────────────
	//  이벤트
	// ─────────────────────────────────────────────────

	/**
	 * Feature 등록/해제 시 브로드캐스트.
	 * 캐싱 포인터를 보유한 시스템이 구독하여 TWeakObjectPtr 유효성을 재검증한다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Ex|DataCenter")
	FOnDataCenterUpdated OnDataCenterUpdated;

protected:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:

	// ─────────────────────────────────────────────────
	//  내부 저장소 (현재 규모: DA 20개 미만 → 자체 TMap 직접 관리)
	//  확장 방침: DA 50개 이상 시 UAssetManager 기반으로 교체 (외부 API 시그니처 불변)
	// ─────────────────────────────────────────────────

	/** Config DA 저장소. Key: 구체 클래스 포인터, Value: 해당 Config DA */
	UPROPERTY()
	TMap<TObjectPtr<UClass>, TObjectPtr<UExConfigDataAsset>> ConfigMap;

	/**
	 * Definition DA 저장소.
	 * 복합키 구조: UClass* → (FGameplayTag → DA*)
	 * 동일 클래스 내 태그 유일성이 보장된다.
	 */
	TMap<TObjectPtr<UClass>, TMap<FGameplayTag, TObjectPtr<UExDefinitionDataAsset>>> DefinitionMap;

	/**
	 * Preset DA 저장소.
	 * 복합키 구조: UClass* → (FGameplayTag → DA*)
	 * 타입별로 공간이 분리되므로 RulePreset과 SpawnPreset이 동일 모드 태그를 가져도 충돌하지 않는다.
	 */
	TMap<TObjectPtr<UClass>, TMap<FGameplayTag, TObjectPtr<UExPresetDataAsset>>> PresetMap;

	/** Feature별 등록 키 추적 — UnregisterByFeature 일괄 해제에 사용 */
	struct FExRegisteredEntry
	{
		UClass* Class = nullptr;
		FGameplayTag Tag;
		enum class EType : uint8 { Config, Definition, Preset } EntryType;
	};
	TMap<FName, TArray<FExRegisteredEntry>> FeatureRegistryMap;

	/**
	 * 데이터 미등록 상황을 개발자에게 즉각 알린다.
	 * ensureMsgf + 화면 디버그 메시지(Shipping/DedicatedServer 빌드에서는 출력하지 않음).
	 */
	void ReportMissingData(const FString& Message) const;
};
