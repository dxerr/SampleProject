// Copyright ExFrameWork. All Rights Reserved.

#include "ExRunnerInputStrategy_Manual.h"
#include "ExRunnerInputComponent.h"
#include "ExRunnerMovementComponent.h"

void UExRunnerInputStrategy_Manual::HandleHorizontalInput(const FVector2D& AxisValue)
{
	if (!OwnerInput) return;

	// [핵심] 기존 수동 모드 조향 복구
	// 스와이프(RequestLookAction)와 가상 조이스틱 모두 Strategy 통합 함수(HandleHorizontalInput)로 유입되므로,
	// 실제 캐릭터를 회전시키는 OnLookRequested 델리게이트를 여기서 수동으로 호출해야 정상적으로 좌/우 조향이 가능합니다.
	OwnerInput->OnMoveRequested.Broadcast(AxisValue);
	OwnerInput->OnLookRequested.Broadcast(AxisValue.X);
}

void UExRunnerInputStrategy_Manual::BindToMovement(UExRunnerMovementComponent* MovementComp)
{
	if (!MovementComp || !OwnerInput) return;

	// OnLookRequested → MovementComponent의 자유 Yaw 회전 콜백 연결
	MovementComp->BindLookInput(OwnerInput);
}

void UExRunnerInputStrategy_Manual::UnbindFromMovement(UExRunnerMovementComponent* MovementComp)
{
	if (!MovementComp || !OwnerInput) return;

	// 중복 바인딩 방지를 위해 기존 Look 연결 해제
	OwnerInput->OnLookRequested.RemoveDynamic(MovementComp, &UExRunnerMovementComponent::OnLookRequestedCallback);
}
