// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FExMovementSettings.generated.h"

USTRUCT(BlueprintType)
struct EXRUNNERPLAYRUNTIME_API FExMovementSettings
{
	GENERATED_BODY()

public:
	// 레인 이동 모드에서 한 레인의 좌우 폭 (단위: cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ExRunner|Movement")
	float LaneWidth = 100.0f;

	// 이산(Discrete) 레인 변경 시의 이동 속도(1초당 레인 이동 비율) - 값이 클수록 빠름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ExRunner|Movement")
	float LaneChangeSpeed = 10.0f;
};
