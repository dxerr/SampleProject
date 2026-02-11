// Copyright ExFrameWork. All Rights Reserved.
// Slide(슬라이딩/Crouch 통과) 장애물 스폰 전략

#pragma once

#include "CoreMinimal.h"
#include "ExObstacleSpawnStrategy.h"
#include "ExObstacleStrategy_Slide.generated.h"

/**
 * UExObstacleStrategy_Slide
 * 슬라이딩/Crouch 통과 장애물 배치 전략
 *
 * - 장애물 하단: Floor.Z + MaxPassHeight (Crouch 캡슐 높이보다 약간 높게)
 * - 아래 공간으로 슬라이딩/웅크려서 통과
 * - X축: 두께 (MinSize.X ~ MaxSize.X 랜덤)
 * - Z축: 장애물 높이 (MinSize.Z ~ MaxSize.Z 랜덤)
 * - Y축: ChunkFloor 너비에 자동 맞춤
 */
UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced, DisplayName = "Slide 전략")
class EXRUNNERPLAYRUNTIME_API UExObstacleStrategy_Slide : public UExObstacleSpawnStrategy
{
	GENERATED_BODY()

public:
	/** Crouch 캡슐 상단과 장애물 하단 사이 여유 공간 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Obstacle|Slide",
		meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm"))
	float ClearanceMargin = 15.f;

	// 장애물 스케일 설정 (X: 두께 랜덤, Y: 바닥 폭, Z: 높이 랜덤)
	virtual void ConfigureObstacle_Implementation(AActor* Obstacle,
	                                              const UExObstacleDefinition* Def,
	                                              AExFloorChunk* Chunk) override;

	// 스폰 위치 계산 (Z = Floor.Z + MaxPassHeight)
	virtual FVector CalculateSpawnPosition_Implementation(const UExObstacleDefinition* Def,
	                                                      AExFloorChunk* Chunk,
	                                                      float SafeStartX) override;
};
