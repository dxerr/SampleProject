// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../Struct/FExPathSegment.h"
#include "../Struct/FExObstacleContext.h"
#include "ExObstacleManager.generated.h"

class AExFloorChunk;
class UExChunkSpawner;
class UExObstacleDefinition;
class UExObstacleSpawnStrategy;
class UExPathManager;
enum class EExObstacleType : uint8;

/**
 * UExObstacleManager
 * 청크 스포너의 이벤트(OnChunkSpawned)를 수신하여
 * 바닥 위에 장애물을 배치하고 관리하는 컴포넌트.
 */
UCLASS(ClassGroup=(ExCore), meta=(BlueprintSpawnableComponent))
class EXRUNNERPLAYRUNTIME_API UExObstacleManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UExObstacleManager();

	/**
	 * 장애물 정의 목록
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Obstacle")
	TArray<UExObstacleDefinition*> ObstacleDefinitions;

	/**
	 * 타입별 스폰 전략 매핑 (Strategy Pattern)
	 * 에디터에서 각 EExObstacleType에 대응하는 전략을 인라인 편집 가능
	 */
	UPROPERTY(EditAnywhere, Instanced, Category = "Obstacle|Strategy")
	TMap<EExObstacleType, TObjectPtr<UExObstacleSpawnStrategy>> SpawnStrategies;

	/**
	 * 연결된 청크 스포너 
	 * (장애물 스폰 확률, 쿨타임 등의 통합 설정 참조용)
	 */
	UPROPERTY(Transient)
	UExChunkSpawner* BoundSpawner;

	/**
	 * 청크 스포너와 연결 (델리게이트 구독)
	 */
	UFUNCTION(BlueprintCallable, Category = "Obstacle")
	void BindToSpawner(UExChunkSpawner* Spawner);

	/**
	 * 비트 기반 장애물 스폰 요청 (BeatSyncComponent에서 호출)
	 */
	UFUNCTION(BlueprintCallable, Category = "Obstacle")
	void RequestBeatSpawn();

	/** 기존 청크 스폰 시 발생되는 장애물 배치를 무시할지 여부 (비트 동기화 시 true) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Obstacle")
	bool bSuppressDefaultChunkSpawn = false;

protected:
	virtual void BeginPlay() override;

	/**
	 * 청크가 생성되었을 때 호출 (장애물 배치)
	 */
	UFUNCTION()
	void OnChunkSpawned(AExFloorChunk* Chunk);

	/**
	 * 청크가 제거되기 직전에 호출 (장애물 회수)
	 */
	UFUNCTION()
	void OnChunkDespawned(AExFloorChunk* Chunk);

	/**
	 * 월드 시프트 이벤트 핸들러 (좌표 보정)
	 */

protected:
	// 장애물 풀: 클래스별로 스택 관리
	// UPROPERTY가 없어도 World에 Spawn된 Actor는 GC되지 않음 (Level이 참조)
	TMap<UClass*, TArray<AActor*>> ObstaclePool;



	// 내부 함수들
	AActor* GetObstacleFromPool(UClass* ObstacleClass);
	void ReturnObstacleToPool(AActor* Obstacle);

	void ActivateObstacle(AActor* Obstacle);
	void DeactivateObstacle(AActor* Obstacle);

public:
	void SpawnObstaclesOnChunk(AExFloorChunk* Chunk, float ChunkStartLocalX, float ChunkLength, bool bForceSpawn = false);

	/**
	 * 특정 PathDistance 근처에 장애물이 있는지 질의한다.
	 * UExRunnerItemManager가 아이템 Z축 배치를 결정할 때 호출.
	 * @param PathDist 질의할 경로 기반 거리
	 * @param QueryRadius 검색 반경 (cm)
	 * @param OutContext 결과가 채워지는 구조체
	 * @return 장애물이 발견되었으면 true
	 */
	UFUNCTION(BlueprintCallable, Category = "Obstacle")
	bool QueryObstacleAtDistance(float PathDist, float QueryRadius, FExObstacleContext& OutContext) const;

protected:
	bool CheckFeasibility(float CurrentSpawnX, float& OutNextSafeX);
	UExObstacleDefinition* SelectRandomDefinition() const;

	// Helper
	static FBoxSphereBounds GetVisualBounds(AActor* Actor);

	// ── 커브 구간 특수 배치 ──

	/**
	 * 커브 구간에서의 장애물 배치 제한 체크
	 * @param Chunk 배치 대상 청크
	 * @return 장애물 배치 허용 여부
	 */


	/** 경로 거리 기반 안전 배치 거리 */
	float LastObstacleSafeEndDistance = -99999.f;
};
