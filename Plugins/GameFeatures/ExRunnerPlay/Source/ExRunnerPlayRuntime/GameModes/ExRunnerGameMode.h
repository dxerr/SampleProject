// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExGameModeBase.h"
#include "GameplayTagContainer.h"
#include "ExRunnerGameMode.generated.h"

class UExChunkSpawner;
class UExObstacleManager;
class UExBeatSyncComponent;
class UExPathManager;
class UExCurveConfig;
class UExBGMTrackDataAsset;
class UExMusicPhaseDataAsset;
struct FExGameplayEventPayload;

/**
 * AExRunnerGameMode
 * 러너 게임 전용 모드
 * 오프셋 기반 트레드밀: BaseSpeed로 Floor를 이동하되,
 * 캐릭터 위치 오프셋에 따라 속도를 부드럽게 가변 조정
 * 커브 경로 지원: PathManager를 통한 경로 기반 이동 + 캐릭터 회전
 */
UCLASS()
class EXRUNNERPLAYRUNTIME_API AExRunnerGameMode : public AExGameModeBase
{
	GENERATED_BODY()

public:
	AExRunnerGameMode();

	// ========== 런타임 상태 ==========

	/** 러너 모드 활성화 여부 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Runner")
	bool bRunnerModeEnabled = true;

	/** 러너 게임 시작 */
	UFUNCTION(BlueprintCallable, Category = "Runner")
	void StartRunnerGame();

	/** 러너 게임 중지 */
	UFUNCTION(BlueprintCallable, Category = "Runner")
	void StopRunnerGame();

	/** 캐싱된 PlayerPawn 반환 */
	UFUNCTION(BlueprintCallable, Category = "Runner")
	APawn* GetCachedPlayerPawn();

	// ── Traversal 상태 (Climb 등) 감지용 ──
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Runner|State")
	bool bIsTraversing = false;

	// 태그 이벤트 콜백 (등반 시작/종료 감지)
	UFUNCTION()
	void OnTraversalStart(FGameplayTag EventTag, const FExGameplayEventPayload& Payload);
	
	UFUNCTION()
	void OnTraversalEnd(FGameplayTag EventTag, const FExGameplayEventPayload& Payload);

	/** 이번 스테이지에 사용할 BGM 데이터 로드 (곡, 속도, 박자, 특수 믹싱 통합 에셋) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Runner|Music")
	TObjectPtr<UExBGMTrackDataAsset> CurrentStageBGM;

	/** 러너 인게임 Phase 전환 */
	UFUNCTION(BlueprintCallable, Category = "Runner|Music")
	void SetRunnerPhase(FGameplayTag NewPhase);
protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	virtual void OnMatchStarted_Implementation() override;
	virtual void OnMatchEnded_Implementation() override;



private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UExChunkSpawner> ChunkSpawner;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UExObstacleManager> ObstacleManager;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UExBeatSyncComponent> BeatSyncComponent;

	/** 커브 설정 데이터 에셋 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Runner|Curve", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UExCurveConfig> CurveConfig;

	/** PlayerPawn 캐시 */
	TWeakObjectPtr<APawn> CachedPlayerPawn;

	/**
	 * 캐릭터 회전 갱신 (경로 접선 방향)
	 */
	void UpdateCharacterRotation(float DeltaTime);

};
