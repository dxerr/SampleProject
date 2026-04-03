// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ExRunnerInputStrategy.generated.h"

class UExRunnerInputComponent;
class UExRunnerMovementComponent;

/**
 * UExRunnerInputStrategy
 * 러너 입력 모드별 행동을 정의하는 Strategy 베이스 클래스.
 *
 * - UObject 기반 (UCLASS): 에디터에서 TSubclassOf로 지정 가능
 * - InputComponent의 Outer로 NewObject 생성 → GC 수명 자동 관리
 * - 네트워크: 순수 클라이언트 로컬 객체, 리플리케이션 불필요
 */
UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class EXRUNNERPLAYRUNTIME_API UExRunnerInputStrategy : public UObject
{
	GENERATED_BODY()

public:
	/** 초기화: InputComponent 참조를 보관합니다 */
	virtual void Initialize(UExRunnerInputComponent* InOwner);

	/**
	 * 좌우(수평) Move 입력 해석
	 * Manual  : OnMoveRequested.Broadcast 전달
	 * AutoRun : OnLaneChangeRequested(±1) 1회 발송
	 */
	virtual void HandleHorizontalInput(const FVector2D& AxisValue) PURE_VIRTUAL(UExRunnerInputStrategy::HandleHorizontalInput, );

	/**
	 * 이산(Discrete) 레인 변경 요청 처리
	 * AutoButtonRun: UI 버튼에서 직접 방향값(-1/+1) 전달
	 * 기본 구현: 아무 동작 안함 (호환 유지)
	 */
	virtual void HandleLaneChangeRequest(int32 LaneDirection);

	/**
	 * 점프 요청 게이트
	 * @return true: 요청 통과 / false: 차단 (쿨다운 등)
	 * 기본 구현: 항상 허용
	 */
	virtual bool CanRequestJump(bool bIsTriggered);

	/**
	 * 슬라이드 요청 게이트
	 * 기본 구현: 항상 허용
	 */
	virtual bool CanRequestSlide(bool bIsTriggered);

	/**
	 * 스프린트 요청 게이트
	 * 기본 구현: 항상 허용
	 */
	virtual bool CanRequestSprint(bool bIsTriggered);

	/**
	 * MovementComponent에 모드별 델리게이트 바인딩
	 * Manual  : OnLookRequested 바인딩
	 * AutoRun : OnLaneChangeRequested 바인딩 + bIsAutoRunMode = true
	 */
	virtual void BindToMovement(UExRunnerMovementComponent* MovementComp) PURE_VIRTUAL(UExRunnerInputStrategy::BindToMovement, );

	/** MovementComponent 바인딩 해제 (모드 전환 시 반드시 호출) */
	virtual void UnbindFromMovement(UExRunnerMovementComponent* MovementComp);

protected:
	/** 소유 InputComponent (InputComponent의 Outer이므로 raw ptr 사용 안전) */
	UPROPERTY()
	TObjectPtr<UExRunnerInputComponent> OwnerInput;
};
