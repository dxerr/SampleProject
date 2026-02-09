// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * ExFrameWork 프로젝트 전용 GameplayTag 정의
 * C++ Native Tag로 정의하여 타입 안전성 및 에디터 자동완성 지원
 * EXCORERUNTIME_API 추가로 다른 모듈에서도 참조 가능
 */

// ========== Action Tags ==========
// Climb 행동 관련 태그
EXCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ex_Action_Climb_Start);
EXCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ex_Action_Climb_End);

// Vault 행동 관련 태그 (향후 확장용)
EXCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ex_Action_Vault_Start);
EXCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ex_Action_Vault_End);

// Slide 행동 관련 태그 (향후 확장용)
EXCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ex_Action_Slide_Start);
EXCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ex_Action_Slide_End);

