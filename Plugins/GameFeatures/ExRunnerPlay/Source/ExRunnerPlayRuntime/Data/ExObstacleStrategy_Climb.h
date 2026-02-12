// Copyright ExFrameWork. All Rights Reserved.
// Climb(매달려 올라가기) 장애물 스폰 전략

#pragma once

#include "CoreMinimal.h"
#include "ExObstacleSpawnStrategy.h"
#include "ExObstacleStrategy_Climb.generated.h"

/**
 * UExObstacleStrategy_Climb
 * 매달려 올라가기 장애물 배치 전략
 * - 현재는 기본(공통) 로직을 그대로 사용
 * - 향후 머리 높이 배치, Z축 중심 스케일 등 고유 로직 추가 예정
 */
UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced, DisplayName = "Climb 전략")
class EXRUNNERPLAYRUNTIME_API UExObstacleStrategy_Climb : public UExObstacleSpawnStrategy
{
	GENERATED_BODY()

	// Climb 전용 로직 (머리 높이, ClimbHeight 기반)
	virtual void ConfigureObstacle_Implementation(AActor* Obstacle, const UExObstacleDefinition* Def, AExFloorChunk* Chunk) override;
};
