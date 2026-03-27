// Copyright ExFrameWork. All Rights Reserved.

#include "ExRunnerInputStrategy_AutoRun.h"
#include "ExRunnerInputComponent.h"
#include "ExRunnerMovementComponent.h"
#include "Data/Modes/ExGameModeDataSet.h"

void UExRunnerInputStrategy_AutoRun::Initialize(UExRunnerInputComponent* InOwner)
{
	Super::Initialize(InOwner);

	// GetGameModeDataSet() 게터를 통해 쿨다운 값 로드 (없으면 기본값 0.3초 유지)
	if (InOwner)
	{
		if (UExGameModeDataSet* DataSet = InOwner->GetGameModeDataSet())
		{
			ActionCooldown = DataSet->AutoRunActionCooldown;
		}
	}
}

void UExRunnerInputStrategy_AutoRun::HandleHorizontalInput(const FVector2D& AxisValue)
{
	if (!OwnerInput) return;

	// 좌우 스냅 판정 임계치 (0.5 이상이면 입력으로 인식)
	const float LaneThreshold = 0.5f;

	// 디버그
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(81, 0.0f, FColor::Cyan, FString::Printf(TEXT("[AutoRun Strategy] Axis.X: %.2f | bLeft: %d | bRight: %d"), AxisValue.X, bLeftTriggered, bRightTriggered));
	}

	// 좌측 레인 이동 요청 (X < -임계치)
	if (AxisValue.X < -LaneThreshold)
	{
		if (!bLeftTriggered)
		{
			if (GEngine) GEngine->AddOnScreenDebugMessage(82, 2.0f, FColor::Yellow, TEXT("--> Broadcasting Lane Change (-1)"));
			bLeftTriggered = true;
			bRightTriggered = false; // 반대 방향 플래그 해제
			OwnerInput->OnLaneChangeRequested.Broadcast(-1);
		}
	}
	// 우측 레인 이동 요청 (X > +임계치)
	else if (AxisValue.X > LaneThreshold)
	{
		if (!bRightTriggered)
		{
			if (GEngine) GEngine->AddOnScreenDebugMessage(82, 2.0f, FColor::Yellow, TEXT("--> Broadcasting Lane Change (+1)"));
			bRightTriggered = true;
			bLeftTriggered = false; // 반대 방향 플래그 해제
			OwnerInput->OnLaneChangeRequested.Broadcast(1);
		}
	}
	else
	{
		// 중립 복귀 시 플래그 초기화 → 다음 입력 허용
		bLeftTriggered = false;
		bRightTriggered = false;
	}
}

bool UExRunnerInputStrategy_AutoRun::CanRequestJump(bool bIsTriggered)
{
	if (!bIsTriggered) return true; // 해제 이벤트는 항상 통과

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (Now - LastJumpTime < ActionCooldown)
	{
		return false; // 쿨다운 중 차단
	}
	LastJumpTime = Now;
	return true;
}

bool UExRunnerInputStrategy_AutoRun::CanRequestSlide(bool bIsTriggered)
{
	if (!bIsTriggered) return true; // 해제 이벤트는 항상 통과

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (Now - LastSlideTime < ActionCooldown)
	{
		return false; // 쿨다운 중 차단
	}
	LastSlideTime = Now;
	return true;
}

void UExRunnerInputStrategy_AutoRun::BindToMovement(UExRunnerMovementComponent* MovementComp)
{
	if (!MovementComp || !OwnerInput) return;

	// 1. 자유 Yaw 회전 바인딩 해제 (경로 정방향 자동 추적만 사용)
	OwnerInput->OnLookRequested.RemoveDynamic(MovementComp, &UExRunnerMovementComponent::OnLookRequestedCallback);

	// 2. 레인 변경 요청 콜백 연결
	OwnerInput->OnLaneChangeRequested.AddDynamic(MovementComp, &UExRunnerMovementComponent::OnLaneChangeRequestedCallback);

	// 3. AutoRun 모드 플래그 활성화 → UpdateLanePosition 보간 연산 허용
	MovementComp->SetAutoRunMode(true);
}

void UExRunnerInputStrategy_AutoRun::UnbindFromMovement(UExRunnerMovementComponent* MovementComp)
{
	if (!MovementComp || !OwnerInput) return;

	// 레인 변경 콜백 해제
	OwnerInput->OnLaneChangeRequested.RemoveDynamic(MovementComp, &UExRunnerMovementComponent::OnLaneChangeRequestedCallback);

	// AutoRun 모드 해제
	MovementComp->SetAutoRunMode(false);
}
