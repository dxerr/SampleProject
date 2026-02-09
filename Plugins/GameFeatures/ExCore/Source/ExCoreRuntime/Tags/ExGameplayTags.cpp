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
