// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExObstacleDefinition.h"
#include "FExObstacleContext.generated.h"

/**
 * 아이템 매니저가 장애물 매니저에게 질의하여 받는 장애물 컨텍스트 정보.
 * 특정 PathDistance 지점에 어떤 장애물이 있는지, 크기와 높이 특성을 담는다.
 */
USTRUCT(BlueprintType)
struct EXRUNNERPLAYRUNTIME_API FExObstacleContext
{
	GENERATED_BODY()

	/** 장애물 존재 여부 */
	UPROPERTY(BlueprintReadOnly, Category = "Obstacle")
	bool bHasObstacle = false;

	/** 장애물 타입 */
	UPROPERTY(BlueprintReadOnly, Category = "Obstacle")
	EExObstacleType ObstacleType = EExObstacleType::None;

	/** 장애물의 월드 바운드 */
	UPROPERTY(BlueprintReadOnly, Category = "Obstacle")
	FBox ObstacleBounds = FBox(ForceInit);

	/** 장애물 꼭대기 Z 좌표 (월드 스페이스) */
	UPROPERTY(BlueprintReadOnly, Category = "Obstacle")
	float ObstacleTopZ = 0.f;

	/** 장애물 하단 Z 좌표 (월드 스페이스, Slide 장애물의 경우 바닥과의 간격 계산용) */
	UPROPERTY(BlueprintReadOnly, Category = "Obstacle")
	float ObstacleBottomZ = 0.f;

	/** 이 장애물을 타고 올라갈 수 있는지 여부 (외부 입력, Slide 타입에서 사용) */
	UPROPERTY(BlueprintReadOnly, Category = "Obstacle")
	bool bCanClimbOver = false;

	/** 올라갈 수 있는 경우의 높이 임계값 (cm) */
	UPROPERTY(BlueprintReadOnly, Category = "Obstacle")
	float ClimbableHeightThreshold = 200.f;
};
