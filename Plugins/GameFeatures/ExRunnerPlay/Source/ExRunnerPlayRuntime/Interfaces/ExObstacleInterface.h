#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "../Data/ExObstacleSpawnStrategy.h" // FExObstacleInfo 참조
#include "ExObstacleInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI, BlueprintType)
class UExObstacleInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 장애물 액터가 구현해야 할 인터페이스
 * SpawnStrategy로부터 장애물 정보를 전달받는 통로
 */
class EXRUNNERPLAYRUNTIME_API IExObstacleInterface
{
	GENERATED_BODY()

public:
	/**
	 * 장애물 정보를 설정합니다.
	 * 
	 * @param Info   설정할 장애물 정보 (타입, 크기 등)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Obstacle")
	void SetupObstacleInfo(const FExObstacleInfo& Info);
};
