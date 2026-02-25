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
 * - 장애물 하단: Floor.Z + (Def->MinSize.Z ~ Def->MaxSize.Z 랜덤 통과 높이)
 * - 아래 공간으로 슬라이딩/웅크려서 통과
 * - X축: 두께 (MinSize.X ~ MaxSize.X 랜덤)
 * - Y축: ChunkFloor 너비에 자동 맞춤
 * - Z축(두께): 1.0 ~ 1.5배 랜덤 스케일
 */
UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced, DisplayName = "Slide 전략")
class EXRUNNERPLAYRUNTIME_API UExObstacleStrategy_Slide : public UExObstacleSpawnStrategy
{
	GENERATED_BODY()

public:

	// 장애물 스케일 설정 (X: 길이 랜덤, Y: 바닥 폭, Z: 두께 1~1.5 고정)
	virtual void ConfigureObstacle_Implementation(AActor* Obstacle,
	                                              const UExObstacleDefinition* Def,
	                                              AExFloorChunk* Chunk) override;

	// 스폰 위치 계산 (Z = Floor.Z + PassHeight)
	virtual FTransform CalculateSpawnPosition_Implementation(const UExObstacleDefinition* Def,
	                                                      AExFloorChunk* Chunk,
	                                                      float SafeStartX) override;
};
