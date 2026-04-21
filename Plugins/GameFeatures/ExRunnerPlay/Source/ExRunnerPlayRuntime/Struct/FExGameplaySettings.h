// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FExGameplaySettings.generated.h"

USTRUCT(BlueprintType)
struct EXRUNNERPLAYRUNTIME_API FExGameplaySettings
{
	GENERATED_BODY()

public:
	// 최대 러너 좌우 이동 각도 제한 (정면 기준, 예: 45도)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ExRunner|Gameplay")
	float MaxRunnerYawAngle = 45.0f;

	// 조이스틱 좌우 회전 보간 속도 (FInterpTo Speed, 낮을수록 느리고 부드럽게 복귀)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ExRunner|Gameplay")
	float LookInterpSpeed = 8.0f;

	// 스와이프 발동 임계 비율 (터치패드 세로 크기 대비, 0.05 ~ 1.0, 기본 30%)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ExRunner|Gameplay", meta=(ClampMin="0.05", ClampMax="1.0"))
	float SwipeActivationPercentage = 0.3f;

	// AutoRun 모드 점프/슬라이드 연속 발동 방지 쿨다운 (초). 기본 0.3초
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ExRunner|Gameplay|AutoRun", meta=(ClampMin="0.0", ClampMax="5.0"))
	float AutoRunActionCooldown = 0.3f;

	// 점프 시 곡선 구간을 예측하여 추가로 회전시킬 Yaw 각도 가중치 (예: 1.0 = 예측 커브 100% 반영)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ExRunner|Gameplay|AutoRun")
	float JumpYawPredictionWeight = 1.0f;
};
