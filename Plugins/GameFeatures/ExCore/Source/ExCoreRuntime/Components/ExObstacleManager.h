// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ExObstacleManager.generated.h"

class AExFloorChunk;
class UExChunkSpawner;
class UExObstacleDefinition;

/**
 * UExObstacleManager
 * 청크 스포너의 이벤트(OnChunkSpawned)를 수신하여
 * 바닥 위에 장애물을 배치하고 관리하는 컴포넌트.
 */
UCLASS(ClassGroup=(ExCore), meta=(BlueprintSpawnableComponent))
class EXCORERUNTIME_API UExObstacleManager : public UActorComponent
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
	 * 청크 스포너와 연결 (델리게이트 구독)
	 */
	UFUNCTION(BlueprintCallable, Category = "Obstacle")
	void BindToSpawner(UExChunkSpawner* Spawner);

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
	UFUNCTION()
	void OnWorldShifted(float DeltaX);

protected:
	// 장애물 풀: 클래스별로 스택 관리
	// UPROPERTY가 없어도 World에 Spawn된 Actor는 GC되지 않음 (Level이 참조)
	TMap<UClass*, TArray<AActor*>> ObstaclePool;

	// 마지막으로 배치된 장애물의 끝 지점 (Feasibility Check용)
	float LastObstacleSafeEndX = -99999.f; 

	// 내부 함수들
	AActor* GetObstacleFromPool(UClass* ObstacleClass);
	void ReturnObstacleToPool(AActor* Obstacle);

	void ActivateObstacle(AActor* Obstacle);
	void DeactivateObstacle(AActor* Obstacle);

	void SpawnObstaclesOnChunk(AExFloorChunk* Chunk, float ChunkStartLocalX, float ChunkLength);
	bool CheckFeasibility(float CurrentSpawnX, float& OutNextSafeX);

	// Helper
	static FBoxSphereBounds GetVisualBounds(AActor* Actor);
};
