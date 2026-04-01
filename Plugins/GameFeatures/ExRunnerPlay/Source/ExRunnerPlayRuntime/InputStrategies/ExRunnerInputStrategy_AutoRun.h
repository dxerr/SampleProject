// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExRunnerInputStrategy.h"
#include "ExRunnerInputStrategy_AutoRun.generated.h"

/**
 * UExRunnerInputStrategy_AutoRun
 * 자동 달리기 모드 — 3레인 스냅 이동 + 쿨다운 기반 액션 게이트
 *
 * 동작 방식:
 * - 좌우 입력 → OnLaneChangeRequested(-1 또는 +1) 1회만 발송
 *   (키를 떼기 전까지 재발동 차단: bLeftTriggered / bRightTriggered)
 * - 점프/슬라이드 → ActionCooldown 초 내 연속 발동 차단
 * - 경로 추적 회전은 MovementComponent의 PathManager가 자동 처리
 *   (OnLookRequested 바인딩 해제, TargetLookYawOffset=0 유지)
 */
UCLASS(DisplayName = "자동 달리기 모드")
class EXRUNNERPLAYRUNTIME_API UExRunnerInputStrategy_AutoRun : public UExRunnerInputStrategy
{
	GENERATED_BODY()

public:
	virtual void Initialize(UExRunnerInputComponent* InOwner) override;
	virtual void HandleHorizontalInput(const FVector2D& AxisValue) override;
	virtual bool CanRequestJump(bool bIsTriggered) override;
	virtual bool CanRequestSlide(bool bIsTriggered) override;
	virtual void BindToMovement(UExRunnerMovementComponent* MovementComp) override;
	virtual void UnbindFromMovement(UExRunnerMovementComponent* MovementComp) override;

private:
	/** 점프/슬라이드 연속 발동 쿨다운 (초). DataSet의 AutoRunActionCooldown 또는 기본값 사용 */
	float ActionCooldown = 0.3f;

	/** 글로벌 마지막 액션(점프/슬라이드/좌우 차선변경) 발생 시각 (GetWorld()->GetTimeSeconds() 기준) */
	float LastActionTime = -999.f;

	/**
	 * BindToMovement에서 캐싱한 MovementComponent 포인터
	 * 레인 보간 완료 여부 및 현재 인덱스 조회에 사용
	 */
	UPROPERTY()
	TWeakObjectPtr<UExRunnerMovementComponent> CachedMovementComp;
};
