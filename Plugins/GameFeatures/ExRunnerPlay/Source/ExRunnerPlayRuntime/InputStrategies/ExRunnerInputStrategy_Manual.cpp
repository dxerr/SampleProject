// Copyright ExFrameWork. All Rights Reserved.

#include "ExRunnerInputStrategy_Manual.h"
#include "ExRunnerInputComponent.h"
#include "ExRunnerMovementComponent.h"

void UExRunnerInputStrategy_Manual::HandleHorizontalInput(const FVector2D& AxisValue)
{
	if (!OwnerInput) return;

	// [핵심] 기존 NativeOnMoveAction 동작 그대로 유지:
	// - Move 이동값만 브로드캐스트
	// - Look(회전)은 스와이프/RequestLookAction 별도 경로가 담당하므로 여기서 절대 호출하지 않음
	OwnerInput->OnMoveRequested.Broadcast(AxisValue);
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
