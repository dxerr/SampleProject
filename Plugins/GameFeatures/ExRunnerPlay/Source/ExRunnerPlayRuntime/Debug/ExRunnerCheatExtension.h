// Copyright ExFrameWork. All Rights Reserved.
// ExRunnerPlay Feature 전용 치트 Extension — Runner 게임 디버깅

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "ExRunnerCheatExtension.generated.h"

/**
 * UExRunnerCheatExtension
 * ExRunnerPlay Feature 전용 디버그 명령
 * 
 * UGameFeatureAction_AddCheats를 통해 ExRunnerPlay 활성화 시 자동 등록
 * Core의 ExDebugStateSubsystem을 참조하지만 역방향 의존성 없음
 * 
 * 콘솔 명령:
 *   ExRunnerShowPath           — 경로 디버그 시각화 토글
 *   ExRunnerShowChunk          — 청크 상태 시각화 토글
 *   ExRunnerForceSlope         — 꽈배기 강제 발동 토글
 *   ExRunnerSetSlopeTrigger N  — SlopeTriggerCount 오버라이드
 *   ExRunnerShowHeightOffset   — HeightOffset 수치 실시간 로그
 */
UCLASS()
class EXRUNNERPLAYRUNTIME_API UExRunnerCheatExtension : public UCheatManagerExtension
{
	GENERATED_BODY()

public:
	// ========== 경로/청크 시각화 ==========

	/** 경로 스플라인 시각화 토글 — TAG_Ex_Debug_Path */
	UFUNCTION(Exec)
	void ExRunnerShowPath();

	/** 청크 경계 및 상태 시각화 토글 — TAG_Ex_Debug_Chunk */
	UFUNCTION(Exec)
	void ExRunnerShowChunk();

	// ========== 꽈배기(경사) 디버그 ==========

	/** 꽈배기 강제 발동 토글 — TAG_Ex_Debug_Slope */
	UFUNCTION(Exec)
	void ExRunnerForceSlope();

	/** SlopeTriggerCount 오버라이드 (0 = 원래값 복원) */
	UFUNCTION(Exec)
	void ExRunnerSetSlopeTrigger(int32 Count = 0);

	/** HeightOffset 실시간 로그 출력 토글 — TAG_Ex_Debug_Slope 연동 */
	UFUNCTION(Exec)
	void ExRunnerShowHeightOffset();

	// ========== 속도 디버그 ==========

	/** Runner 속도 정보 실시간 표시 토글 — TAG_Ex_Debug_Speed */
	UFUNCTION(Exec)
	void ExRunnerShowSpeed();
};
