// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFeatureAction.h"
#include "GameFeaturesSubsystem.h"
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
 * [핵심 타이밍 설계]
 *   패키징된 게임에서 FWorldDelegates::OnStartGameInstance 는 엔진 초기화 중
 *   GameFeature 활성화보다 일찍 발화하므로 DataCenter 등록에 사용할 수 없다.
 *   대신 FWorldDelegates::OnWorldBeginPlay 를 구독한다.
 *   이 이벤트는 각 게임 월드의 BeginPlay 진입 시점에 발화하며,
 *   UExDataCenterSubsystem::Initialize() 이후가 보장된다.
 */
UCLASS(meta=(DisplayName="Add Ex Data"))
class EXCORERUNTIME_API UGameFeatureAction_AddExData : public UGameFeatureAction
{
	GENERATED_BODY()

public:

	/**
	 * 이 Feature에서 DataCenter에 등록할 Config DA.
	 * 모듈당 1개를 여기에 지정한다.
	 */
	UPROPERTY(EditAnywhere, Category = "DataCenter|Config")
	TObjectPtr<UExConfigDataAsset> ConfigAsset;

	/**
	 * 이 Feature에서 DataCenter에 등록할 Definition DA 목록.
	 */
	UPROPERTY(EditAnywhere, Category = "DataCenter|Definitions")
	TArray<TObjectPtr<UExDefinitionDataAsset>> DefinitionAssets;

	/**
	 * 이 Feature에서 DataCenter에 등록할 Preset DA 목록.
	 */
	UPROPERTY(EditAnywhere, Category = "DataCenter|Presets")
	TArray<TObjectPtr<UExPresetDataAsset>> PresetAssets;

	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;

private:
	void AddToDataCenter(UGameInstance* GameInstance, const FGameFeatureStateChangeContext& Context);
	void RemoveFromDataCenter(UGameInstance* GameInstance, const FGameFeatureStateChangeContext& Context);

	/**
	 * FWorldDelegates::OnWorldBeginPlay 핸들러.
	 * 새 게임 월드가 BeginPlay에 진입할 때 호출되어 DataCenter를 채운다.
	 * OnStartGameInstance 보다 늦게 발화하므로 패키징 빌드에서도 안전하다.
	 */
	void HandleWorldBeginPlay(UWorld* World, FGameFeatureStateChangeContext ChangeContext);

	/** OnWorldBeginPlay 바인딩 핸들 목록 (비활성화 시 일괄 해제) */
	TArray<FDelegateHandle> WorldBeginPlayHandles;

	/** PIE 등 이미 존재하는 GameInstance에 대한 즉시 처리 후
	 *  추가로 생성될 인스턴스를 위한 OnStartGameInstance 핸들 (보조) */
	TArray<FDelegateHandle> GameInstanceStartHandles;
};
