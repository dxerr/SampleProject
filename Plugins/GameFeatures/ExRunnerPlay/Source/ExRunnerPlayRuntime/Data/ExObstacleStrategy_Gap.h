// Copyright ExFrameWork. All Rights Reserved.
// Gap(점프 구간) 장애물 스폰 전략

#pragma once

#include "CoreMinimal.h"
#include "ExObstacleSpawnStrategy.h"
#include "ExObstacleStrategy_Gap.generated.h"

/**
 * UExObstacleStrategy_Gap
 * 바닥이 없는 구간(Gap) 장애물 배치 전략
 * - 현재는 기본(공통) 로직을 그대로 사용
 * - 향후 바닥 메시 숨김, 허공 배치 등 고유 로직 추가 예정
 */
UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced, DisplayName = "Gap 전략")
class EXRUNNERPLAYRUNTIME_API UExObstacleStrategy_Gap : public UExObstacleSpawnStrategy
{
	GENERATED_BODY()

	// TODO: 향후 ConfigureObstacle / CalculateSpawnPosition 오버라이드하여
	//       Gap 전용 로직 (바닥 숨김, 허공 배치) 구현 예정
};
