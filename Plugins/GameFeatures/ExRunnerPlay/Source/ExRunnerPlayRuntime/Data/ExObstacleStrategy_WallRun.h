// Copyright ExFrameWork. All Rights Reserved.
// WallRun(벽 달리기) 장애물 스폰 전략

#pragma once

#include "CoreMinimal.h"
#include "ExObstacleSpawnStrategy.h"
#include "ExObstacleStrategy_WallRun.generated.h"

/**
 * UExObstacleStrategy_WallRun
 * 벽 달리기 장애물 배치 전략
 * - 현재는 기본(공통) 로직을 그대로 사용
 * - 향후 측면 벽 생성, Y축 오프셋 등 고유 로직 추가 예정
 */
UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced, DisplayName = "WallRun 전략")
class EXRUNNERPLAYRUNTIME_API UExObstacleStrategy_WallRun : public UExObstacleSpawnStrategy
{
	GENERATED_BODY()

	// TODO: 향후 ConfigureObstacle / CalculateSpawnPosition 오버라이드하여
	//       WallRun 전용 로직 (측면 벽, Y축 오프셋) 구현 예정
};
