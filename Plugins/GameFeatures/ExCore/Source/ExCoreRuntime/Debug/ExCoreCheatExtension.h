// Copyright ExFrameWork. All Rights Reserved.
// ExCore 범용 치트 Extension — Feature 무관 공통 디버그 명령

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "ExCoreCheatExtension.generated.h"

/**
 * UExCoreCheatExtension
 * ExCore 범용 디버그 명령 모음 (Feature 무관)
 * 
 * UGameFeatureAction_AddCheats를 통해 ExCore GameFeature 활성화 시 자동 등록
 * 
 * 콘솔 명령 예시:
 *   ExGodMode          — 무적 모드 토글
 *   ExSetSpeed 2000    — 이동속도 강제 설정
 *   ExSlowMo 0.5       — 글로벌 타임 딜레이션
 *   ExShowDebugAll     — 모든 디버그 카테고리 활성화
 */
UCLASS()
class EXCORERUNTIME_API UExCoreCheatExtension : public UCheatManagerExtension
{
	GENERATED_BODY()

public:
	// ========== 플레이어 상태 치트 ==========

	/** 무적 모드 토글 — TAG_Ex_Debug_GodMode */
	UFUNCTION(Exec)
	void ExGodMode();

	/** 이동속도 강제 설정 (0 = 원래값 복원) */
	UFUNCTION(Exec)
	void ExSetSpeed(float Speed = 0.f);

	// ========== 월드 치트 ==========

	/** 글로벌 타임 딜레이션 (1.0 = 정상, 0.5 = 슬로모) */
	UFUNCTION(Exec)
	void ExSlowMo(float TimeDilation = 1.f);

	// ========== 디버그 유틸리티 ==========

	/** 모든 디버그 카테고리 활성화/비활성화 토글 */
	UFUNCTION(Exec)
	void ExShowDebugAll();

	/** 특정 카테고리 디버그 활성화 (값 전달 가능) */
	UFUNCTION(Exec)
	void ExSetDebugValue(FString Category, float Value);

	// ========== UI 시스템 (Popup / Toast) 테스트 ==========

	UFUNCTION(Exec)
	void ExUITestInfo(FString Title = TEXT("테스트"), FString Body = TEXT("안내 팝업입니다."));

	UFUNCTION(Exec)
	void ExUITestConfirm(FString Title = TEXT("확인"), FString Body = TEXT("진행하시겠습니까?"));

	UFUNCTION(Exec)
	void ExUITestToast(FString Message = TEXT("업적 플래티넘 달성!"));

private:
	/** 전체 디버그 토글 상태 추적 */
	bool bAllDebugEnabled = false;
};
