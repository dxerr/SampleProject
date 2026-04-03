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

	// ExGameModeDataSet의 SwipeActivationPercentage를 공통 임계치로 사용
	const float LaneThreshold = OwnerInput->GetSwipeActivationPercentage();

	// 수직(점프/슬라이드) 스와이프 도중 미세한 가로 흔들림으로 인한 레인 이동 방지
	// X축보다 Y축 이동량이 더 크면 가로 입력으로 취급하지 않음 (대각선 오입력 방지)
	if (FMath::Abs(AxisValue.Y) > FMath::Abs(AxisValue.X))
	{
		return;
	}

	UExRunnerMovementComponent* MovComp = CachedMovementComp.Get();
	if (!MovComp) return;

	// 디버그 표시
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(81, 0.0f, FColor::Cyan,
			FString::Printf(TEXT("[AutoRun Strategy] Axis.X: %.2f | Threshold: %.2f | Lane: %d | Comp: %d"),
				AxisValue.X, LaneThreshold, MovComp->GetCurrentLaneIndex(), MovComp->IsLaneTransitionComplete()));
	}

	// ─── 고정 임계값 기반 연속 이동 로직 (주인님 지시사항 반영) ───────────────────
	
	// 1. 우측 드래그 (X > +임계치)
	if (AxisValue.X > LaneThreshold)
	{
		// 보간이 완료되었고, 더 이상 우측으로 갈 수 있는 레인이 있다면 즉시 요청
		if (MovComp->IsLaneTransitionComplete() && MovComp->GetCurrentLaneIndex() < 1)
		{
			if (GEngine) GEngine->AddOnScreenDebugMessage(82, 2.0f, FColor::Yellow, TEXT("--> Lane Change (+1) Requested"));
			OwnerInput->OnLaneChangeRequested.Broadcast(1);
		}
	}
	// 2. 좌측 드래그 (X < -임계치)
	else if (AxisValue.X < -LaneThreshold)
	{
		// 보간이 완료되었고, 더 이상 좌측으로 갈 수 있는 레인이 있다면 즉시 요청
		if (MovComp->IsLaneTransitionComplete() && MovComp->GetCurrentLaneIndex() > -1)
		{
			if (GEngine) GEngine->AddOnScreenDebugMessage(82, 2.0f, FColor::Yellow, TEXT("--> Lane Change (-1) Requested"));
			OwnerInput->OnLaneChangeRequested.Broadcast(-1);
		}
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

	// 3. AutoRun 모드 플래그 활성화하여 UpdateLanePosition 보간 연산 허용
	MovementComp->SetAutoRunMode(true);
	MovementComp->SetUseDirectLateralMovement(false); // AutoRun은 물리 스티어링 사용

	// 4. 보간 완료 여부 및 레인 인덱스 조회를 위해 포인터 캐싱
	CachedMovementComp = MovementComp;
}

void UExRunnerInputStrategy_AutoRun::UnbindFromMovement(UExRunnerMovementComponent* MovementComp)
{
	if (!MovementComp || !OwnerInput) return;

	// 레인 변경 콜백 해제
	OwnerInput->OnLaneChangeRequested.RemoveDynamic(MovementComp, &UExRunnerMovementComponent::OnLaneChangeRequestedCallback);

	// AutoRun 모드 해제
	MovementComp->SetAutoRunMode(false);
	MovementComp->SetUseDirectLateralMovement(false);

	// 캐싱 포인터 해제
	CachedMovementComp = nullptr;
}
