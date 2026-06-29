// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExGameModeBase.h"
#include "GameplayTagContainer.h"
#include "ExRunnerGameMode.generated.h"

class UExChunkSpawner;
class UExObstacleManager;
class UExRunnerItemManager;
class UExBeatSyncComponent;
class UExRunnerConfig;
class UExPathManager;
class UExPathManager;
class UExBGMTrackDataAsset;
class UExMusicPhaseDataAsset;
class UExRunnerRuleManagerComponent;
class UExExperienceManagerComponent;
struct FExGameplayEventPayload;
class UShapeComponent;

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

	/**
	 * FallDeath 룰로부터 호출되는 Kill Volume 스폰
	 * @param KillVolumeZ Kill Volume을 배치할 Z 좌표 (cm)
	 * @return 생성된 UBoxComponent 포인터
	 */
	UShapeComponent* SpawnKillVolume(float KillVolumeZ);
protected:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	virtual void OnMatchStarted_Implementation() override;

	/** TimeUp/GoalReached 룰 발동 시 EventSubsystem 콜백 → SetMatchPhase(PostMatch) */
	UFUNCTION()
	void OnRuleEndGameEvent(FGameplayTag EventTag, const FExGameplayEventPayload& Payload);

	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

public:
	virtual void OnMatchEnded_Implementation() override;

	// 매치 플로우 설정 (오버라이드)
	virtual int32 GetExpectedPlayerCount() const override;
	virtual int32 GetCountdownDuration() const override;
	virtual float GetMaxWaitForPlayersSeconds() const override;

	/** 전멸 검사 - Individual 룰 발동 후 생존자가 없으면 매치 종료 처리 */
	UFUNCTION(BlueprintCallable, Category = "Runner|Rule")
	void CheckAlivePlayers();

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UExBeatSyncComponent> BeatSyncComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UExRunnerRuleManagerComponent> RuleManagerComponent;

	/** DataCenter로부터 할당받은 RunnerConfig 의 약참조 */
	TWeakObjectPtr<UExRunnerConfig> RunnerConfig;

	/** DataCenter 초기화 완료 여부 (중복 실행 방지) */
	bool bDataCenterInitialized = false;

	/** PlayerPawn 캐시 */
	TWeakObjectPtr<APawn> CachedPlayerPawn;

	/** 서버 전용: 레인 할당을 위한 인덱스 */
	int32 NextLaneSlotIndex = 0;

	/** 초기 맵 데이터 스폰 (Chunk 등) 동기적 수행 */
	void PrewarmRunnerWorld();

	/**
	 * Experience 로드 완료 콜백.
	 * ExExperienceManagerComponent::OnExperienceLoadCompleteEvent 에 바인딩되어,
	 * GameFeature가 완전히 활성화되어 DataCenter가 준비된 시점에 호출된다.
	 * DataCenter 조회 및 PrewarmRunnerWorld 호출은 반드시 이 시점 이후에 수행해야 한다.
	 */
	void OnExperienceReady();

	/**
	 * DataCenter 업데이트 콜백.
	 * UExDataCenterSubsystem::OnDataCenterUpdated 에 바인딩된다.
	 * RegisterConfig가 ConfigMap에 데이터를 추가한 직후 발화하므로,
	 * GetConfig 호출이 반드시 성공이 보장된 가장 안전한 진입점이다.
	 * HasConfig<T>() 가드를 통해 원하는 타입이 등록됐을 때만 초기화를 진행한다.
	 */
	UFUNCTION()
	void OnDataCenterUpdated_GameMode();

	/**
	 * DataCenter에서 RunnerConfig를 안전하게 조회하여 초기화를 시도한다.
	 * HasConfig<T>()로 먼저 가드하므로 데이터 미등록 시 에러를 출력하지 않는다.
	 * 초기화 성공 시 OnDataCenterUpdated 구독을 해제하고 PrewarmRunnerWorld를 호출한다.
	 */
	void TryInitRunnerFromDataCenter();

	/**
	 * 캐릭터 회전 갱신 (경로 접선 방향)
	 */
	void UpdateCharacterRotation(float DeltaTime);

	/** UExBeatSyncComponent::OnBeatTick 구독 핸들러 — ObstacleManager에 비트 스폰 요청 */
	UFUNCTION()
	void OnBeatTick_Handler(int32 BeatIndex, float ElapsedTime);

	/** UExBeatSyncComponent::OnBeatSyncStateChanged 구독 핸들러 — ObstacleManager 기본 스폰 억제 갱신 */
	UFUNCTION()
	void OnBeatSyncStateChanged_Handler(bool bEnabled);

};
