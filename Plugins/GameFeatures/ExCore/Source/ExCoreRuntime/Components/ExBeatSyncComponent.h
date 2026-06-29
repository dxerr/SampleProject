// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ExBeatSyncSettings.h"
#include "ExBeatSyncComponent.generated.h"

struct FExGameplayEventPayload;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FExOnBeatTick, int32, BeatIndex, float, ElapsedTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FExOnBeatSyncStateChanged, bool, bEnabled);

/**
 * UExBeatSyncComponent
 * 음악 비트 이벤트(TAG_Music_Beat)를 수신하고 OnBeatTick 델리게이트를 브로드캐스트하는 범용 컴포넌트.
 * ObstacleManager 등 Feature 전용 시스템은 OnBeatTick을 구독하여 자체적으로 처리합니다.
 * 설정은 InitSettings()로 외부에서 주입합니다.
 */
UCLASS(ClassGroup = (ExCore), meta = (BlueprintSpawnableComponent))
class EXCORERUNTIME_API UExBeatSyncComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UExBeatSyncComponent();

	/**
	 * 비트 확률 필터를 통과한 비트마다 발화.
	 * BeatIndex: 누적 비트 수신 카운터, ElapsedTime: 발화 시점의 월드 시간(초).
	 */
	UPROPERTY(BlueprintAssignable, Category = "BeatSync")
	FExOnBeatTick OnBeatTick;

	/** 비트 동기화 활성/비활성 전환 시 발화 */
	UPROPERTY(BlueprintAssignable, Category = "BeatSync")
	FExOnBeatSyncStateChanged OnBeatSyncStateChanged;

	/** 비트 동기화 활성화 여부 (런타임 제어용) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BeatSync")
	bool bRuntimeBeatSyncEnabled = true;

	/**
	 * BeatSync 설정을 외부에서 주입합니다. BeginPlay 전후 언제든 호출 가능.
	 * @param Settings RunnerConfig 등 Feature 데이터에서 가져온 설정 구조체
	 */
	UFUNCTION(BlueprintCallable, Category = "BeatSync")
	void InitSettings(const FExBeatSyncSettings& Settings);

	/**
	 * 비트 동기화 활성화 여부를 변경합니다. OnBeatSyncStateChanged 델리게이트를 발화합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BeatSync")
	void SetBeatSyncEnabled(bool bEnabled);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnMusicBeat(FGameplayTag EventTag, const FExGameplayEventPayload& Payload);

private:
	FExBeatSyncSettings CurrentSettings;

	/** 누적 비트 수신 카운터 (확률 통과 여부 무관하게 매 비트마다 증가) */
	int32 BeatIndex = 0;

	float LastBeatFireTime = -999.0f;
	float MinBeatFireInterval = 0.2f;
};
