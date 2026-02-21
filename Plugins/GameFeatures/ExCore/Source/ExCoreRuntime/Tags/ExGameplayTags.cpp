// Copyright ExFrameWork. All Rights Reserved.

#include "ExGameplayTags.h"

/**
 * ExFrameWork GameplayTag 정의
 * UE_DEFINE_GAMEPLAY_TAG_COMMENT 매크로로 태그 문자열과 설명을 함께 정의
 */

// ========== Action Tags ==========
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ex_Action_Climb_Start, "Ex.Action.Climb.Start", "Climb 행동 시작 - 트레드밀 정지 트리거");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ex_Action_Climb_End, "Ex.Action.Climb.End", "Climb 행동 종료 - 트레드밀 재개 트리거");

UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ex_Action_Vault_Start, "Ex.Action.Vault.Start", "Vault 행동 시작");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ex_Action_Vault_End, "Ex.Action.Vault.End", "Vault 행동 종료");

UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ex_Action_Slide_Start, "Ex.Action.Slide.Start", "Slide 행동 시작");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ex_Action_Slide_End, "Ex.Action.Slide.End", "Slide 행동 종료");

// ========== Debug Tags ==========
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ex_Debug_Path, "Ex.Debug.Path", "경로 시각화 토글");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ex_Debug_Chunk, "Ex.Debug.Chunk", "청크 경계/상태 시각화 토글");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ex_Debug_Slope, "Ex.Debug.Slope", "꽈배기(경사) 디버그 토글");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ex_Debug_Speed, "Ex.Debug.Speed", "속도 디버그 토글");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ex_Debug_GodMode, "Ex.Debug.GodMode", "무적 모드 토글");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ex_Debug_Collision, "Ex.Debug.Collision", "충돌 시각화 토글");
