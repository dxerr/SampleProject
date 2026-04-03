// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExRunnerInputStrategy_AutoRun.h"
#include "ExRunnerInputStrategy_AutoButtonRun.generated.h"

/**
 * UExRunnerInputStrategy_AutoButtonRun
 * 자동 달리기 모드 (4방향 버튼 특화)
 * 
 * 동작 방식:
 * - 드래그/스와이프 축 입력을 무시합니다.
 * - 버튼 이벤트(RequestLaneChange)를 통해 수신된 방향(-1, +1)으로 즉각(쿨타임 없이) 레인 이동을 Broadcast 합니다.
 * - 상하(점프/슬라이드) 액션은 부모의 쿨타임 정책을 그대로 사용합니다.
 */
UCLASS(DisplayName = "자동 달리기 모드 (4방향 버튼)")
class EXRUNNERPLAYRUNTIME_API UExRunnerInputStrategy_AutoButtonRun : public UExRunnerInputStrategy_AutoRun
{
	GENERATED_BODY()

public:
	/** 드래그 입력 무시 — 이 모드에서는 버튼만 사용하므로 처리하지 않습니다 */
	virtual void HandleHorizontalInput(const FVector2D& AxisValue) override;

	/** 버튼 기반 이산 레인 변경 (보간 완료 + 레인 범위 체크 후 Broadcast) - 쿨다운 없음 */
	virtual void HandleLaneChangeRequest(int32 LaneDirection) override;

	/** MovementComponent 바인딩 시 물리 횡이동을 끄고 강제 보간 모드 활성화 */
	virtual void BindToMovement(class UExRunnerMovementComponent* MovementComp) override;
};
