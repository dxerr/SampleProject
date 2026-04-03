// Copyright ExFrameWork. All Rights Reserved.

#include "ExRunnerInputStrategy_AutoButtonRun.h"
#include "Components/ExRunnerInputComponent.h"
#include "Components/ExRunnerMovementComponent.h"

void UExRunnerInputStrategy_AutoButtonRun::HandleHorizontalInput(const FVector2D& AxisValue)
{
	// 드래그/스와이프 입력을 의도적으로 무시합니다.
	// AutoButtonRun 모드에서는 UI에서 명시적으로 찍은 버튼(HandleLaneChangeRequest)만 레인 이동을 결정합니다.
}

void UExRunnerInputStrategy_AutoButtonRun::HandleLaneChangeRequest(int32 LaneDirection)
{
	if (!OwnerInput) return;

	UExRunnerMovementComponent* MovComp = CachedMovementComp.Get();
	if (!MovComp) return;

	// 레인 간 보간 중에는 추가 입력을 무시하여 정확한 이동 단위를 보장합니다.
	if (!MovComp->IsLaneTransitionComplete()) return;

	// 레인 범위 체크 (양 끝 레인에서 바깥으로 진행하려는 입력 무시)
	// 보통 레인 인덱스는 -1, 0, 1 을 사용합니다.
	if (LaneDirection > 0 && MovComp->GetCurrentLaneIndex() >= 1) return;
	if (LaneDirection < 0 && MovComp->GetCurrentLaneIndex() <= -1) return;

	// 쿨다운 체크를 제거하여 버튼 연타(단일 프레임 즉각적인 반응) 보장

	OwnerInput->OnLaneChangeRequested.Broadcast(LaneDirection);
}

void UExRunnerInputStrategy_AutoButtonRun::BindToMovement(class UExRunnerMovementComponent* MovementComp)
{
	// 부모(AutoRun)의 공통 로직 호출 (AutoRun=true 등)
	Super::BindToMovement(MovementComp);

	if (MovementComp)
	{
		// [개선] AutoButtonRun 모드 진입 시 물리력이 개입하는 스티어링을 차단하고 
		// 즉각적이고 안정적인 SetActorLocation 보정을 사용하도록 설정
		MovementComp->SetUseDirectLateralMovement(true);
	}
}
