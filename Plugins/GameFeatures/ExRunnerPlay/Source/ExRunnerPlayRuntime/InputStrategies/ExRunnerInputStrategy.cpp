// Copyright ExFrameWork. All Rights Reserved.

#include "ExRunnerInputStrategy.h"

void UExRunnerInputStrategy::Initialize(UExRunnerInputComponent* InOwner)
{
	OwnerInput = InOwner;
}

bool UExRunnerInputStrategy::CanRequestJump(bool bIsTriggered)
{
	// 기본 구현: 제한 없음
	return true;
}

bool UExRunnerInputStrategy::CanRequestSlide(bool bIsTriggered)
{
	// 기본 구현: 제한 없음
	return true;
}

bool UExRunnerInputStrategy::CanRequestSprint(bool bIsTriggered)
{
	// 기본 구현: 제한 없음
	return true;
}

void UExRunnerInputStrategy::UnbindFromMovement(UExRunnerMovementComponent* MovementComp)
{
	// 기본 구현: 아무것도 하지 않음
	// 파생 클래스에서 필요 시 오버라이드하여 델리게이트 해제
}
