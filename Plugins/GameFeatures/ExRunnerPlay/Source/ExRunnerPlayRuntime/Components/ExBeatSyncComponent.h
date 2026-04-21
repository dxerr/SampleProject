// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ExBeatSyncComponent.generated.h"

class UExObstacleManager;
struct FExGameplayEventPayload;

/**
 * UExBeatSyncComponent
 * 음악 비트 이벤트(TAG_Music_Beat 등)를 수신하여 장애물 스폰을 요청하는 컴포넌트
 */
UCLASS(ClassGroup=(ExRunner), meta=(BlueprintSpawnableComponent))
class EXRUNNERPLAYRUNTIME_API UExBeatSyncComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UExBeatSyncComponent();

	/** 매 비트마다 장애물 스폰을 시도할 확률 (DataCenter 캐시용) */
	UPROPERTY(Transient)
	TObjectPtr<class UExRunnerConfig> CachedConfig;

	/** 비트 동기화 활성화 여부 (런타임 제어용, 초기값은 Config 따름) */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "BeatSync")
	bool bRuntimeBeatSyncEnabled = true;

	/**
	 * 비트 동기화 활성화 여부 변경 (블루프린트 런타임 제어용)
	 */
	UFUNCTION(BlueprintCallable, Category = "BeatSync")
	void SetBeatSyncEnabled(bool bEnabled);

	/** 대상 ObstacleManager 바인딩 */
	UFUNCTION(BlueprintCallable, Category = "BeatSync")
	void BindToObstacleManager(UExObstacleManager* InManager);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 비트 이벤트 수신 핸들러 */
	UFUNCTION()
	void OnMusicBeat(FGameplayTag EventTag, const FExGameplayEventPayload& Payload);

private:
	UPROPERTY()
	TObjectPtr<UExObstacleManager> BoundObstacleManager;

	float LastBeatSpawnTime;
	float MinBeatSpawnInterval;
};
