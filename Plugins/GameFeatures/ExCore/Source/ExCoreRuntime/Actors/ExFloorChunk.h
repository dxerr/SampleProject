// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ExFloorChunk.generated.h"

class AExCoreGameMode;

/**
 * AExFloorChunk
 * 러너 게임의 이동하는 바닥 청크
 * 트레드밀 메커니즘: 플레이어는 고정, 바닥이 -X 방향으로 이동
 */
UCLASS()
class EXCORERUNTIME_API AExFloorChunk : public AActor
{
	GENERATED_BODY()

public:
	AExFloorChunk();

	/**
	 * 바닥 메시 컴포넌트
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> FloorMesh;

	/**
	 * 청크가 삭제될 X 좌표 (플레이어 뒤쪽)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Runner")
	float KillZ = -2000.f;

	/**
	 * 청크 길이 (X축, 다음 청크 스폰 위치 계산용)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Runner")
	float ChunkLength = 1000.f;

	/**
	 * 오브젝트 풀에서 관리 중인지 여부
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Runner")
	bool bIsPooled = false;

	/**
	 * 청크 활성화 (풀에서 꺼낼 때)
	 */
	UFUNCTION(BlueprintCallable, Category = "Floor")
	void ActivateChunk(const FVector& SpawnLocation);

	/**
	 * 청크 비활성화 (풀로 반환)
	 */
	UFUNCTION(BlueprintCallable, Category = "Floor")
	void DeactivateChunk();

	/**
	 * 풀로 반환 (게임모드에 알림)
	 */
	UFUNCTION(BlueprintCallable, Category = "Floor")
	void ReturnToPool();

	UFUNCTION(BlueprintPure, Category = "Floor")
	FBox GetFloorBounds() const;

	/**
	 * 청크가 KillZ에 도달했을 때 호출되는 델리게이트
	 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChunkReachedKillZ, AExFloorChunk*, Chunk);
	UPROPERTY(BlueprintAssignable, Category = "Runner")
	FOnChunkReachedKillZ OnChunkReachedKillZ;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	/**
	 * 캐시된 게임모드 참조
	 */
	UPROPERTY()
	TObjectPtr<AExCoreGameMode> CachedGameMode;
};
