// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFeatureAction.h"
#include "GameFeatureAction_AddExData.generated.h"

class UExConfigDataAsset;
class UExDefinitionDataAsset;
class UExPresetDataAsset;

/**
 * GameFeatureAction_AddExData — DataCenter 데이터 자동 등록/해제 액션.
 *
 * 역할:
 *   GameFeature 플러그인이 활성화될 때 해당 Feature의 DataAsset 들을
 *   UExDataCenterSubsystem에 자동으로 등록한다.
 *   비활성화 시 등록된 데이터를 일괄 해제한다.
 *
 * 의존성 방향 보장:
 *   ExCore의 DataCenter는 ExRunnerPlay의 존재를 알 필요가 없다.
 *   Feature 쪽에서 자신의 데이터를 DataCenter에 "밀어넣는(Push)" 방식이므로
 *   Core → Feature 의존성이 생기지 않는다.
 *
 * 사용 방법:
 *   GameFeature의 .uasset(GameFeatureData) 에디터에서 Actions에 이 액션을 추가하고
 *   ConfigAsset, DefinitionAssets, PresetAssets 배열에 해당 DA를 지정한다.
 *
 * 현재 참조 방식:
 *   GameFeature가 활성화되면 어차피 모든 DA가 메모리에 로딩되므로
 *   초기에는 TObjectPtr(하드 참조)로 구현한다.
 *
 * 확장 방침:
 *   DA가 50개 이상으로 확장되면 TSoftObjectPtr + 비동기 로드 방식으로 전환한다.
 */
UCLASS(meta=(DisplayName="Add Ex Data"))
class EXCORERUNTIME_API UGameFeatureAction_AddExData : public UGameFeatureAction
{
	GENERATED_BODY()

public:

	/**
	 * 이 Feature에서 DataCenter에 등록할 Config DA.
	 * 모듈당 1개를 여기에 지정한다.
	 * 예: DA_ExConfig_Runner
	 */
	UPROPERTY(EditAnywhere, Category = "DataCenter|Config")
	TObjectPtr<UExConfigDataAsset> ConfigAsset;

	/**
	 * 이 Feature에서 DataCenter에 등록할 Definition DA 목록.
	 * 매니페스트 DA 없이 직접 개별 Definition들을 나열한다.
	 * 예: DA_ExItem_Coin, DA_ExItem_SpeedBoost, DA_ExObstacle_Gap ...
	 */
	UPROPERTY(EditAnywhere, Category = "DataCenter|Definitions")
	TArray<TObjectPtr<UExDefinitionDataAsset>> DefinitionAssets;

	/**
	 * 이 Feature에서 DataCenter에 등록할 Preset DA 목록.
	 * 모드·난이도별 DA들을 나열한다.
	 * 예: DA_ExPreset_EndlessMode, DA_ExPreset_EasyDifficulty ...
	 */
	UPROPERTY(EditAnywhere, Category = "DataCenter|Presets")
	TArray<TObjectPtr<UExPresetDataAsset>> PresetAssets;

	/** GameFeature 활성화 시 DataCenter에 모든 지정 데이터를 등록한다. */
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;

	/** GameFeature 비활성화 시 DataCenter에서 이 Feature가 등록한 데이터를 일괄 해제한다. */
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
};
