// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExRunnerInputStrategy.h"
#include "ExRunnerInputStrategy_Manual.generated.h"

/**
 * UExRunnerInputStrategy_Manual
 * 수동 입력 모드 — 기존 360도 자유 Yaw 회전 + 제한 없는 액션
 *
 * 동작 방식:
 * - HandleHorizontalInput: NativeOnMoveAction 기존 로직 그대로,
 *   OnMoveRequested.Broadcast 만 수행 (Look과 엄격히 분리)
 * - Look(회전)은 별도 터치스와이프/RequestLookAction 경로를 통해서만 처리
 * - BindToMovement: OnLookRequestedCallback 바인딩 (자유 Yaw 회전 적용)
 */
UCLASS(DisplayName = "수동 입력 모드")
class EXRUNNERPLAYRUNTIME_API UExRunnerInputStrategy_Manual : public UExRunnerInputStrategy
{
	GENERATED_BODY()

public:
	virtual void HandleHorizontalInput(const FVector2D& AxisValue) override;
	virtual void BindToMovement(UExRunnerMovementComponent* MovementComp) override;
	virtual void UnbindFromMovement(UExRunnerMovementComponent* MovementComp) override;
};
