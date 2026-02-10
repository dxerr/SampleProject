// Copyright ExFrameWork. All Rights Reserved.
// Slide(슬라이딩) 장애물 스폰 전략

#pragma once

#include "CoreMinimal.h"
#include "ExObstacleSpawnStrategy.h"
#include "ExObstacleStrategy_Slide.generated.h"

/**
 * UExObstacleStrategy_Slide
 * 슬라이딩 장애물 배치 전략
 * - 현재는 기본(공통) 로직을 그대로 사용
 * - 향후 낮은 천장, MaxPassHeight 제한 등 고유 로직 추가 예정
 */
UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced, DisplayName = "Slide 전략")
class EXRUNNERPLAYRUNTIME_API UExObstacleStrategy_Slide : public UExObstacleSpawnStrategy
{
	GENERATED_BODY()

	// TODO: 향후 ConfigureObstacle / CalculateSpawnPosition 오버라이드하여
	//       Slide 전용 로직 (낮은 천장, MaxPassHeight) 구현 예정
};
