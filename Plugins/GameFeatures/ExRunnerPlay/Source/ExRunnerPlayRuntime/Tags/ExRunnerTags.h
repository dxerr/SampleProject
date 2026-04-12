// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * ExRunnerPlay 플러그인 전용 GameplayTag 정의
 * C++ Native Tag로 정의하여 타입 안전성 및 에디터 자동완성 지원
 */

// ========== UI Tags ==========
// 러너 전용 UI 관련 태그
EXRUNNERPLAYRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_UI_Runner_SpeedBar);

// ========== Rule Tags ==========
// 게임 룰 시스템 관련 태그

/** 낙하 사망 룰 발동 */
EXRUNNERPLAYRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Rule_FallDeath);

/** 시간 초과 룰 발동 */
EXRUNNERPLAYRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Rule_TimeUp);

/** 목표 거리 달성 룰 발동 */
EXRUNNERPLAYRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Rule_GoalReached);

/** 타이머 경고 구간 진입 (연동: bIsTimerWarning) */
EXRUNNERPLAYRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Rule_Timer_Warning);

/** 플레이어가 Kill Volume에 진입 시 브로드캐스트 (FallDeath 룰 내부 트리거) */
EXRUNNERPLAYRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Player_DeathVolume);
