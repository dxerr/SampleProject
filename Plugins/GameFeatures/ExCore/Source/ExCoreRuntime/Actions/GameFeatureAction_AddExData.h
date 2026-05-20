// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFeatureAction.h"
#include "GameFeaturesSubsystem.h"
#include "GameFeatureAction_AddExData.generated.h"

class UExConfigDataAsset;
class UExDefinitionDataAsset;
class UExPresetDataAsset;
struct FActorsInitializedParams;

/**
 * GameFeatureAction_AddExData — DataCenter 데이터 자동 등록/해제 액션.
 *
 * [핵심 타이밍 설계]
 *   패키징된 게임에서 FWorldDelegates::OnStartGameInstance 는 엔진 초기화 중
 *   GameFeature 활성화보다 일찍 발화하여 DataCenter 등록에 사용할 수 없다.
 *   대신 FWorldDelegates::OnWorldInitializedActors 를 구독한다.
 *   이 이벤트는 각 게임 월드의 Actor 초기화 완료 시점에 발화하며,
 *   UExDataCenterSubsystem::Initialize() 이후가 보장된다.
 */
UCLASS(meta=(DisplayName="Add Ex Data"))
class EXCORERUNTIME_API UGameFeatureAction_AddExData : public UGameFeatureAction
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category = "DataCenter|Config")
	TObjectPtr<UExConfigDataAsset> ConfigAsset;

	UPROPERTY(EditAnywhere, Category = "DataCenter|Definitions")
	TArray<TObjectPtr<UExDefinitionDataAsset>> DefinitionAssets;

	UPROPERTY(EditAnywhere, Category = "DataCenter|Presets")
	TArray<TObjectPtr<UExPresetDataAsset>> PresetAssets;

	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;

private:
	void AddToDataCenter(UGameInstance* GameInstance, const FGameFeatureStateChangeContext& Context);
	void RemoveFromDataCenter(UGameInstance* GameInstance, const FGameFeatureStateChangeContext& Context);

	/**
	 * FWorldDelegates::OnWorldInitializedActors 핸들러.
	 * 새 게임 월드의 Actor 초기화가 완료되면 호출되어 DataCenter를 채운다.
	 * OnStartGameInstance 보다 늦게 발화하므로 패키징 빌드에서도 안전하다.
	 */
	void HandleWorldInitializedActors(const FActorsInitializedParams& Params, FGameFeatureStateChangeContext ChangeContext);

	/** OnWorldInitializedActors 바인딩 핸들 목록 */
	TArray<FDelegateHandle> WorldInitializedHandles;
};
