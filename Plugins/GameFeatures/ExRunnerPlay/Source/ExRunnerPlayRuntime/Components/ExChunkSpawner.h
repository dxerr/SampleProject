/**
 * @file ExChunkSpawner.h
 * @brief 청크 스폰 및 오브젝트 풀 관리 컴포넌트
 * @details 러너 게임에서 무한 맵 생성을 위한 청크 풀링 시스템
 * 
 * Copyright ExFrameWork. All Rights Reserved.
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ExChunkSpawner.generated.h"

class AExFloorChunk;
class AExCoreGameMode;

/**
 * UExChunkSpawner
 * 러너 게임의 청크 스폰 및 오브젝트 풀 관리 컴포넌트
 * GameMode에 부착하여 사용
 */
UCLASS(ClassGroup=(ExCore), meta=(BlueprintSpawnableComponent))
class EXRUNNERPLAYRUNTIME_API UExChunkSpawner : public UActorComponent
{
	GENERATED_BODY()

public:
	UExChunkSpawner();

	/**
	 * 스폰할 청크 블루프린트 클래스
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spawner")
	TSubclassOf<AExFloorChunk> ChunkClass;

	/**
	 * 초기 풀 크기
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spawner")
	int32 InitialPoolSize = 5;

	/**
	 * 청크 스폰 시작 X 좌표 (플레이어 앞)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spawner")
	float SpawnStartX = 0.f;

	/**
	 * 청크 간 간격 (겹치지 않도록)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spawner")
	float ChunkSpacing = 1000.f;

	/**
	 * 최대 활성 청크 수
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spawner")
	int32 MaxActiveChunks = 10;

	/**
	 * 스포너 초기화 및 초기 청크 배치
	 */
	UFUNCTION(BlueprintCallable, Category = "Spawner")
	void InitializeSpawner();

	/**
	 * 새 청크를 다음 위치에 스폰
	 * @return 스폰된 청크, 실패 시 nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "Spawner")
	AExFloorChunk* SpawnNextChunk();

	/**
	 * 청크를 풀로 반환
	 * @param Chunk 반환할 청크
	 */
	UFUNCTION(BlueprintCallable, Category = "Spawner")
	void ReturnChunkToPool(AExFloorChunk* Chunk);

	/**
	 * 모든 청크 정리
	 */
	UFUNCTION(BlueprintCallable, Category = "Spawner")
	void ClearAllChunks();

	/**
	 * 모든 활성 청크를 X축으로 이동 (World Shift)
	 * @param DeltaX 이동할 X 거리
	 */
	UFUNCTION(BlueprintCallable, Category = "Spawner")
	void ShiftWorld(float DeltaX);

	/**
	 * 청크 생성 시 이벤트 (장애물 매니저 등 외부 시스템 연동용)
	 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChunkEvent, AExFloorChunk*, Chunk);
	
	UPROPERTY(BlueprintAssignable, Category = "Spawner")
	FOnChunkEvent OnChunkSpawned;

	UPROPERTY(BlueprintAssignable, Category = "Spawner")
	FOnChunkEvent OnChunkDespawned;

	/**
	 * 월드 시프트(좌표 이동) 발생 시 이벤트 (DeltaX 만큼 이동)
	 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorldShifted, float, DeltaX);

	UPROPERTY(BlueprintAssignable, Category = "Spawner")
	FOnWorldShifted OnWorldShifted;

	/** 활성 청크 목록 접근자 */
	const TArray<TObjectPtr<AExFloorChunk>>& GetActiveChunks() const { return ActiveChunks; }

protected:
	virtual void BeginPlay() override;

	/**
	 * 청크가 KillZ 도달 시 콜백
	 * @param Chunk KillZ에 도달한 청크
	 */
	UFUNCTION()
	void OnChunkReachedKillZ(AExFloorChunk* Chunk);

private:
	/**
	 * 오브젝트 풀 (비활성 청크)
	 * FIFO 방식: Insert(0)으로 앞에 추가, Pop()으로 뒤에서 꺼냄
	 * 반환된 청크가 최소 1사이클 이상 대기 후 재사용되어 즉시 재사용 버그 방지
	 */
	UPROPERTY()
	TArray<TObjectPtr<AExFloorChunk>> ChunkPool;

	/**
	 * 활성 청크 목록
	 */
	UPROPERTY()
	TArray<TObjectPtr<AExFloorChunk>> ActiveChunks;

	/**
	 * 다음 청크 스폰 위치
	 */
	float NextSpawnX = 0.f;

	/**
	 * 풀에서 청크 가져오기 (없으면 새로 생성)
	 * @return 풀에서 가져온 청크 또는 새로 생성된 청크
	 */
	AExFloorChunk* GetChunkFromPool();

	/**
	 * 새 청크 생성
	 * @return 새로 생성된 청크, 실패 시 nullptr
	 */
	AExFloorChunk* CreateNewChunk();
};
