// Copyright ExFrameWork. All Rights Reserved.
// ExCore Base CheatManager — Feature Extension 관리 허브

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "ExCheatManager.generated.h"

/**
 * UExCheatManager
 * ExFrameWork 프로젝트의 Base CheatManager
 * 
 * 역할:
 * - UCheatManagerExtension 기반 모듈 확장의 진입점
 * - 각 Feature 모듈(ExRunnerPlay 등)은 자신만의 Extension을 등록
 * - PlayerController의 CheatClass에 이 클래스를 지정하여 사용
 * 
 * 사용법:
 * 1. BP_ExPlayerController의 CheatClass에 UExCheatManager 설정
 * 2. 각 Feature의 GameFeatureData에 UGameFeatureAction_AddCheats 추가
 * 3. Extension에서 UFUNCTION(Exec)으로 콘솔 명령 정의
 */
UCLASS()
class EXCORERUNTIME_API UExCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	UExCheatManager();

	// ========== 범용 디버그 유틸리티 ==========

	/**
	 * 디버그 상태 서브시스템에 대한 간편 접근
	 * 콘솔에서 직접 호출 가능한 핵심 명령어
	 */

	/** 카테고리별 디버그 토글 (ex: Ex.Debug Path) */
	UFUNCTION(Exec)
	void ExDebug(FString Category);

	/** 매치 페이즈 강제 변경 (ex: ExSetMatchPhase Match.Playing) */
	UFUNCTION(Exec)
	void ExSetMatchPhase(FString PhaseTagString);

	/** 현재 활성화된 모든 치트 상태를 로그에 출력 */
	UFUNCTION(Exec)
	void ExDebugStatus();

	/** 모든 디버그 상태를 초기화 */
	UFUNCTION(Exec)
	void ExDebugReset();

protected:
	virtual void InitCheatManager() override;
};
