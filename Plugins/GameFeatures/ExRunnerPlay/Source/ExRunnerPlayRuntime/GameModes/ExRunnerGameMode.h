// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExCoreGameMode.h"
#include "GameplayTagContainer.h"
#include "ExRunnerGameMode.generated.h"

class UExChunkSpawner;
class UExObstacleManager;
struct FExGameplayEventPayload;

/**
 * AExRunnerGameMode
 * 러너 게임 전용 모드
 * 오프셋 기반 트레드밀: BaseSpeed로 Floor를 이동하되,
 * 캐릭터 위치 오프셋에 따라 속도를 부드럽게 가변 조정
 */
UCLASS()
class EXRUNNERPLAYRUNTIME_API AExRunnerGameMode : public AExCoreGameMode
{
	GENERATED_BODY()

public:
	AExRunnerGameMode();

	// ========== 트레드밀 설정 ==========

	/** 트레드밀 기본 속도 (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Runner|Treadmill")
	float BaseTreadmillSpeed = 600.f;



	/**
	 * 오프셋 보정 계수
	 * 값이 클수록 기준점 이탈 시 빠르게 복귀
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Runner|Treadmill")
	float CorrectionStrength = 2.0f;



	// ========== 런타임 상태 ==========

	/** 현재 트레드밀 속도 (BaseSpeed + 가속 + 오프셋 보정 적용 후) */
	UPROPERTY(BlueprintReadOnly, Category = "Runner")
	float CurrentTreadmillSpeed = 0.f;

	/** 총 이동 거리 */
	UPROPERTY(BlueprintReadOnly, Category = "Runner")
	float TotalDistance = 0.f;

	/** 러너 모드 활성화 여부 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Runner")
	bool bRunnerModeEnabled = true;

	/** 트레드밀 일시 정지 여부 (Climb 등 상호작용 중) */
	UPROPERTY(BlueprintReadOnly, Category = "Runner")
	bool bTreadmillPaused = false;

	/** 트레드밀 완전 비활성화 여부 */
	UPROPERTY(BlueprintReadOnly, Category = "Runner")
	bool bTreadmillDisabled = false;

	// ========== 함수 ==========

	/** 트레드밀 일시 정지 (재개 시 TargetX 자동 갱신) */
	UFUNCTION(BlueprintCallable, Category = "Runner")
	void SetTreadmillPaused(bool bPaused);

	/** 트레드밀 완전 비활성화/활성화 */
	UFUNCTION(BlueprintCallable, Category = "Runner")
	void SetTreadmillDisabled(bool bDisabled);

	/** 현재 트레드밀 속도 반환 */
	UFUNCTION(BlueprintPure, Category = "Runner")
	float GetCurrentTreadmillSpeed() const { return CurrentTreadmillSpeed; }

	/** 러너 게임 시작 */
	UFUNCTION(BlueprintCallable, Category = "Runner")
	void StartRunnerGame();

	/** 러너 게임 중지 */
	UFUNCTION(BlueprintCallable, Category = "Runner")
	void StopRunnerGame();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// ========== GameplayTag Event Callbacks ==========
	UFUNCTION()
	void OnClimbStart(FGameplayTag EventTag, const FExGameplayEventPayload& Payload);

	UFUNCTION()
	void OnClimbEnd(FGameplayTag EventTag, const FExGameplayEventPayload& Payload);

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UExChunkSpawner> ChunkSpawner;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UExObstacleManager> ObstacleManager;

	// ========== 오프셋 추적 (트레드밀 핵심) ==========

	/** 캐릭터 기준 X 좌표 (스폰 위치) */
	float TargetX = 0.f;

	/** 오프셋 추적 초기화 완료 여부 */
	bool bTrackingInitialized = false;



	/** PlayerPawn 캐시 */
	TWeakObjectPtr<APawn> CachedPlayerPawn;

	/** 캐싱된 PlayerPawn 반환 */
	APawn* GetCachedPlayerPawn();


};
