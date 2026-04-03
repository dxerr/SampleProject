// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "ExRunnerInputViewModel.generated.h"

/**
 * UI 터치 버튼 입력(ViewBinding)을 수신하여 
 * 로컬 플레이어의 UExRunnerInputComponent 로 라우팅해주는 브릿지 뷰모델
 */
UCLASS(BlueprintType)
class EXRUNNERPLAYRUNTIME_API UExRunnerInputViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	// UI 점프 버튼 누름 시 (ViewBinding 이벤트)
	UFUNCTION(BlueprintCallable, Category="ExUI|RunnerInput")
	void OnJumpButtonPressed();

	// UI 점프 버튼 뗌 시 (ViewBinding 이벤트)
	UFUNCTION(BlueprintCallable, Category="ExUI|RunnerInput")
	void OnJumpButtonReleased();

	// UI 슬라이드 버튼 누름 시 (ViewBinding 이벤트)
	UFUNCTION(BlueprintCallable, Category="ExUI|RunnerInput")
	void OnSlideButtonPressed();

	// UI 슬라이드 버튼 뗌 시 (ViewBinding 이벤트)
	UFUNCTION(BlueprintCallable, Category="ExUI|RunnerInput")
	void OnSlideButtonReleased();

	// UI 좌측 레인 이동 버튼 클릭 시 (ViewBinding 이벤트)
	UFUNCTION(BlueprintCallable, Category="ExUI|RunnerInput")
	void OnLeftLaneButtonClicked();

	// UI 우측 레인 이동 버튼 클릭 시 (ViewBinding 이벤트)
	UFUNCTION(BlueprintCallable, Category="ExUI|RunnerInput")
	void OnRightLaneButtonClicked();

	// UI 스프린트 체크박스 상태 변경 시 (ViewBinding 이벤트)
	UFUNCTION(BlueprintCallable, Category="ExUI|RunnerInput")
	void OnSprintCheckStateChanged(bool bIsChecked);

	// 스와이프 발동 비율은 DA_ExGameModeDataSet > GameMode|Runner > SwipeActivationPercentage 에서 관리됩니다.
	// ExRunnerInputComponent::GetSwipeActivationPercentage() 를 통해 참조합니다.

	// 커스텀 터치 패드 이동 (ViewBinding 이벤트)
	// @param FrameDelta        : 이전 프레임 대비 픽셀 이동량 (내부적으로는 미사용, 디버그 가능)
	// @param LocalPosition     : 위젯 내 현재 터치 픽셀 좌표
	// @param NormalizedOffset  : 패드 중앙 기준 -1.0 ~ 1.0 정규화 오프셋
	//                            X = 좌우 회전 입력 | Y = 상하 스와이프 입력
	UFUNCTION(BlueprintCallable, Category="ExUI|RunnerInput")
	void OnTouchPadMoved(FVector2D FrameDelta, FVector2D LocalPosition, FVector2D NormalizedOffset);

private:

	// === 상하 동작 상태 관리 ===
	// 점프 Hold 상태: bIsJumpActive 유지 (InjectedInputStates 기반)
	// 슈라이드 상태: ExRunnerInputComponent::IsSlideInputActive() 로 직접 조회 (bIsSlideActive 제거)
	bool bIsJumpActive = false;

	// === 스와이프 기준점 추적 ===
	bool bIsTouchPadActive = false;  // 첫 프레임 기준점 저장용
	float SwipeStartNormY = 0.0f;    // 터치 시작 NormY (상대 이동량 산출 기준)

	// 점프 입력 처리 (단발성), 발동 시 true 반환
	bool HandleJumpInput(float RelativeY, float Threshold);

	// 슬라이드 입력 처리 (지속형, 임계값 이탈 시 자동 해제)
	void HandleSlideInput(float RelativeY, float Threshold);

	// 터치 패드 입력이 완전히 해제되었을 때(Release) 호출하여 상태 초기화
	UFUNCTION(BlueprintCallable, Category="ExUI|RunnerInput")
	void OnTouchPadReleased();

	// 안전하게 로컬 플레이어의 Runner Input Component를 가져오는 내부 헬퍼
	class UExRunnerInputComponent* GetRunnerInputComponent() const;
};
