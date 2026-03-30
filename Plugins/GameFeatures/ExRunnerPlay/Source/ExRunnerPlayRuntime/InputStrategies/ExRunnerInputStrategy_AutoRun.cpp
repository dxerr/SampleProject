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

	// [개선] ExGameModeDataSet의 SwipeActivationPercentage를 공통 임계치로 사용
	const float LaneThreshold = OwnerInput->GetSwipeActivationPercentage();

	// 수직(점프/슬라이드) 스와이프 도중 미세한 가로 흔들림으로 인한 레인 이동 방지
	// X축보다 Y축 이동량이 더 크면 가로 입력으로 취급하지 않음 (대각선 오입력 방지)
	if (FMath::Abs(AxisValue.Y) > FMath::Abs(AxisValue.X))
	{
		return;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

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
			bLeftTriggered = true;
			bRightTriggered = false; // 반대 방향 플래그 해제

			// 글로벌 쿨다운 락 통과 검사 (동시 액션 차단)
			if (Now - LastActionTime >= ActionCooldown)
			{
				LastActionTime = Now; // 쿨다운 락 온
				if (GEngine) GEngine->AddOnScreenDebugMessage(82, 2.0f, FColor::Yellow, TEXT("--> Broadcasting Lane Change (-1)"));
				OwnerInput->OnLaneChangeRequested.Broadcast(-1);
			}
		}
	}
	// 우측 레인 이동 요청 (X > +임계치)
	else if (AxisValue.X > LaneThreshold)
	{
		if (!bRightTriggered)
		{
			bRightTriggered = true;
			bLeftTriggered = false; // 반대 방향 플래그 해제

			// 글로벌 쿨다운 락 통과 검사 (동시 액션 차단)
			if (Now - LastActionTime >= ActionCooldown)
			{
				LastActionTime = Now; // 쿨다운 락 온
				if (GEngine) GEngine->AddOnScreenDebugMessage(82, 2.0f, FColor::Yellow, TEXT("--> Broadcasting Lane Change (+1)"));
				OwnerInput->OnLaneChangeRequested.Broadcast(1);
			}
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
	if (Now - LastActionTime < ActionCooldown)
	{
		return false; // 글로벌 쿨다운 중 조작 차단
	}
	LastActionTime = Now;
	return true;
}

bool UExRunnerInputStrategy_AutoRun::CanRequestSlide(bool bIsTriggered)
{
	if (!bIsTriggered) return true; // 해제 이벤트는 항상 통과

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (Now - LastActionTime < ActionCooldown)
	{
		return false; // 글로벌 쿨다운 중 조작 차단
	}
	LastActionTime = Now;
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
