// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameModes/ExGameStateBase.h"
#include "Struct/EExRunnerGameOverReason.h"
#include "ExRunnerGameState.generated.h"

class UExPathManager;
class UExChunkSpawner;
class UExObstacleManager;
class UExRunnerItemManager;
class UExBGMTrackDataAsset;

/** Timer 룰 — 잔여 시간(초) 변경 알림 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnRemainingTimeChanged, int32 /*NewSeconds*/);

/** RuleManager — 게임오버 원인 변경 알림 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGameOverReasonChanged, EExRunnerGameOverReason /*Reason*/);

/**
 * AExRunnerGameState
 * 러너 모드 전용 게임 상태 클래스
 * 서버와 클라이언트 모두에서 경로(PathManager)와 이동 거리에 접근 가능하도록 동기화합니다.
 */
UCLASS()
class EXRUNNERPLAYRUNTIME_API AExRunnerGameState : public AExGameStateBase
{
	GENERATED_BODY()

public:
	AExRunnerGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 현재 트레드밀의 누적 이동 거리 (가상 거리)
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "ExRunner|Status")
	float CurrentPathDistance = 0.f;

	// 실제 플레이어의 경로상 위치 (Chunk 삭제 및 곡선 산출용)
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "ExRunner|Status")
	float RealPlayerPathDistance = 0.f;

	UFUNCTION(BlueprintCallable, Category = "ExRunner|Status")
	float GetCurrentPathDistance() const { return CurrentPathDistance; }

	UFUNCTION(BlueprintCallable, Category = "ExRunner|Status")
	float GetPlayerPathDistance() const { return RealPlayerPathDistance; }

	// ── JIP (Join-In-Progress) 및 다중 플레이어 동기화 변수 ──
	
	/** 전체 플레이어 중 가장 앞서가는 거리 */
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "ExRunner|Multiplayer")
	float LeadDistance = 0.f;

	/** 전체 플레이어 중 가장 뒤처진 거리 */
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "ExRunner|Multiplayer")
	float TailDistance = 0.f;

	/** 트랙 생성용 공유 시드 (모든 클라이언트가 같은 맵 시드 사용) */
	UPROPERTY(ReplicatedUsing = OnRep_SharedTrackSeed, VisibleAnywhere, BlueprintReadOnly, Category = "ExRunner|Multiplayer")
	int32 SharedTrackSeed = 0;

	UFUNCTION()
	void OnRep_SharedTrackSeed();

	/** 현재까지 생성된 세그먼트 수 (JIP 클라이언트 추적용) */
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "ExRunner|Multiplayer")
	int32 CurrentSegmentIndex = 0;

	/** 최신 세그먼트의 시작 누적 거리 */
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "ExRunner|Multiplayer")
	float SegmentStartDistance = 0.f;

	/** 청크 정리가 완료된 플로어 거리 기준점 (이 거리 이전의 청크는 JIP가 생성 스킵) */
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "ExRunner|Multiplayer")
	float CleanupWatermark = 0.f;

	/** 현재 재생 중인 스테이지 BGM 정보 (클라이언트 동기화용) */
	UPROPERTY(ReplicatedUsing = OnRep_StageBGM, VisibleAnywhere, BlueprintReadOnly, Category = "ExRunner|Music")
	TObjectPtr<const UExBGMTrackDataAsset> StageBGM;

	UFUNCTION()
	void OnRep_StageBGM();

	/** 서버에서 BGM을 설정 (Standalone/ListenServer 포함 공용) */
	void SetStageBGM(const UExBGMTrackDataAsset* InTrackData);

	// ── 룰 시스템 — Replicated 프로퍼티 ──────────────────────────────

	/** Timer 룰이 초당 1회 갱신 — 클라이언트 HUD 타이머 표시 */
	UPROPERTY(ReplicatedUsing = OnRep_RemainingTime)
	int32 RemainingTimeSeconds = 0;

	/** RuleManager가 게임오버 시 설정 — 클라이언트 UI 분기 */
	UPROPERTY(ReplicatedUsing = OnRep_GameOverReason)
	EExRunnerGameOverReason GameOverReason = EExRunnerGameOverReason::None;

	/** ViewModel이 구독하는 변경 알림 — OnRep에서 Broadcast */
	FOnRemainingTimeChanged  OnRemainingTimeChanged;
	FOnGameOverReasonChanged OnGameOverReasonChanged;

	/** 서버에서 Timer 룰이 호출 */
	void SetRemainingTimeSeconds(int32 NewSeconds);

	/** 서버에서 RuleManager가 호출 */
	void SetGameOverReason(EExRunnerGameOverReason NewReason);

	// 클라이언트 모두 접근할 경로 매니저 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UExPathManager> PathManager;

	// 결정론적 스폰을 위해 GameState로 이관된 로컬 스포너 관리자들
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UExChunkSpawner> ChunkSpawner;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UExObstacleManager> ObstacleManager;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UExRunnerItemManager> ItemManager;

	/** 매 프레임 플레이어 배열을 돌며 Lead/Tail 갱신 */
	virtual void Tick(float DeltaSeconds) override;
	
private:
	UFUNCTION()
	void OnRep_RemainingTime();

	UFUNCTION()
	void OnRep_GameOverReason();
};
