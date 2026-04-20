// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExItemSpawnManagerBase.h"
#include "Struct/FExObstacleContext.h"
#include "GameplayTagContainer.h"
#include "ExRunnerItemManager.generated.h"

class AExFloorChunk;
class UExChunkSpawner;
class UExObstacleManager;
class UExRunnerItemSpawnTable;
class UCurveFloat;

/**
 * UExRunnerItemManager
 * 러너 게임 전용 아이템 배치 매니저.
 * 
 * 중앙 제어 방식(v1.2):
 *   이 매니저는 OnChunkSpawned 이벤트를 직접 듣지 않는다.
 *   UExChunkSpawner가 장애물 배치 완료 후 명시적으로
 *   SpawnItemsOnChunk()를 호출하여 배치 순서를 보장한다.
 *
 * Z축 배치:
 *   장애물 타입(Climb/Slide/Gap)에 따라 자동으로 Z축 위치를 결정하며,
 *   Gap 구간에서는 점프 포물선(Arc) 궤적을 그린다.
 *   곡선 청크에서는 GetLocalTransformAtDistance()를 활용하여
 *   바닥 표면의 굴곡을 자연스럽게 따른다.
 */
UCLASS(ClassGroup = (ExCore), meta = (BlueprintSpawnableComponent))
class EXRUNNERPLAYRUNTIME_API UExRunnerItemManager : public UExItemSpawnManagerBase
{
	GENERATED_BODY()

public:
	UExRunnerItemManager();

	// ── 데이터 ──

	/** 이 러너 스테이지에서 사용할 아이템 스폰 테이블 탐색 태그 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runner|Item", meta = (Categories = "Ex.Runner.SpawnTable"))
	FGameplayTag SpawnTableTag;

protected:
	/** 캐싱된 스폰 테이블 데이터 (DataCenter에서 획득) */
	UPROPERTY(Transient)
	TObjectPtr<UExRunnerItemSpawnTable> CachedSpawnTable;

	/** DataCenter로부터 SpawnTable을 동적 로드 (지연 로딩 지원) */
	void EnsureSpawnTableLoaded();

	// ── Z축 배치 설정 ──

	/** 아이템 기본 Z축 오프셋 (충돌 캡슐의 HalfHeight 등, 바닥에 파묻히지 않도록 올려줌) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runner|Item|Placement")
	float ItemBaseZOffset = 50.f;

	/** Climb/Slide 꼭대기 배치 시 장애물 Top에서의 추가 오프셋 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runner|Item|Placement")
	float ItemTopPlacementOffset = 50.f;

	/** Gap 구간에서 아이템을 배치할 점프 정점 높이 (바닥 기준, cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runner|Item|Placement")
	float JumpApexHeight = 250.f;

	/** 
	 * Gap 구간에서 점프 포물선(Arc) 궤적 커브 (선택적).
	 * X축: 0~1 (Gap 시작~끝 비율), Y축: 0~1 (높이 비율, 1 = JumpApexHeight).
	 * 미할당 시 고정 높이(JumpApexHeight) 사용.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runner|Item|Placement")
	TObjectPtr<UCurveFloat> JumpArcCurve;

	/** Slide 장애물이 올라갈 수 있을 때, 꼭대기에 아이템이 배치될 확률 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runner|Item|Placement",
		meta = (ClampMin = "0", ClampMax = "1"))
	float SlideTopPlacementRatio = 0.3f;

	// ── 중앙 제어 인터페이스 ──
public:

	/**
	 * ChunkSpawner가 장애물 배치를 완료한 후 명시적으로 호출한다.
	 * 내부에서 Chunk의 GetLocalTransformAtDistance()를 사용해 표면 좌표를 잡고,
	 * ObstacleManager에 QueryObstacleAtDistance()로 장애물 정보를 질의하여 Z축을 결정한다.
	 * @param TargetChunk 대상 청크
	 * @param ObstacleManager 장애물 매니저 참조 (질의용)
	 */
	UFUNCTION(BlueprintCallable, Category = "Runner|Item")
	void SpawnItemsOnChunk(AExFloorChunk* TargetChunk, UExObstacleManager* ObstacleManager);

	/** ChunkSpawner의 소멸 이벤트를 구독하여 청크와 함께 제거되는 아이템들을 회수 처리한다. */
	UFUNCTION(BlueprintCallable, Category = "Runner|Item")
	void BindToSpawner(UExChunkSpawner* Spawner);

	/**
	 * 장애물 컨텍스트와 구간 내 비율을 기반으로 아이템의 최종 Z 좌표를 결정한다.
	 * @param Context 장애물 질의 결과
	 * @param ChunkBaseZ 해당 Distance에서 청크의 바닥 기준 Z (곡선 청크 대응)
	 * @param AlphaInGap Gap 장애물 통과 시 포물선 커브 평가 비율 (0~1)
	 * @return 아이템이 배치될 최종 Z 좌표 (월드 스페이스)
	 */
	UFUNCTION(BlueprintPure, Category = "Runner|Item")
	float CalculateItemZ(const FExObstacleContext& Context, float ChunkBaseZ, float AlphaInGap) const;

protected:
	virtual void BeginPlay() override;

	/** 청크 레이아웃이 화면 밖으로 나가 파괴될 때 실행 */
	UFUNCTION()
	void OnChunkDespawned(AExFloorChunk* Chunk);

private:
	/** 안전 배치 거리 추적 (아이템 간 최소 이격 보장) */
	float LastItemSafeEndDistance = -99999.f;

	/** 코인 라인 배치 */
	void SpawnCoinLine(AExFloorChunk* Chunk, UExObstacleManager* ObstacleManager, float StartDistance, float EndDistance);

	/** 코인 객체로부터 실제 충돌 반경을 동적으로 획득하여 캐싱 */
	float GetCachedCoinRadius();

	/** 청크 내 버프 아이템 스폰 (단독 또는 코인 라인 중 삽입) */
	void SpawnBuffItem(AExFloorChunk* Chunk, UExObstacleManager* ObstacleManager, float AtDistance, float LateralOffset = 0.f);

protected:
	/** 코인 획득 반경 캐싱 (여백 계산용) */
	UPROPERTY(Transient)
	float CachedCoinRadius = -1.f;
	/** 뱀 패턴을 끊김 없이 이어가기 위한 지속 상태 변수 */
	UPROPERTY(Transient)
	int32 PersistentTargetLane = 0; // -1, 0, 1 중 하나

	UPROPERTY(Transient)
	int32 RemainingCoinsInCurrentLane = 0; // 현재 타겟 레인에서 스폰해야 할 남은 코인 수

	UPROPERTY(Transient)
	float CurrentLaneYOffset = 0.f; // 현재 보간 중인 실제 Y 오프셋 좌표

	/** 청크 범위를 넘어갈 때, 다음 청크에서 코인이 시작해야 할 오프셋 이월값 */
	UPROPERTY(Transient)
	float PersistentNextCoinDistance = 0.f;
};
