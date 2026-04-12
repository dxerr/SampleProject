// Copyright ExFrameWork. All Rights Reserved.

#include "ExRunnerTags.h"

/**
 * UE_DEFINE_GAMEPLAY_TAG_COMMENT 매크로로 태그 문자열과 설명을 함께 정의
 * 모듈이 로드될 때 엔진의 GameplayTagManager에 자동 등록됩니다.
 */

// ========== UI Tags ==========
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_UI_Runner_SpeedBar, "UI.Runner.SpeedBar", "러너 속도 표시 UI 호출 태그");

// ========== Rule Tags ==========
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Rule_FallDeath,      "Ex.Runner.Rule.FallDeath",       "낙하 사망 룰 발동");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Rule_TimeUp,         "Ex.Runner.Rule.TimeUp",          "시간 초과 룰 발동");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Rule_GoalReached,    "Ex.Runner.Rule.GoalReached",     "목표 거리 달성 룰 발동");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Rule_Timer_Warning,  "Ex.Runner.Rule.Timer.Warning",   "타이머 경고 구간 진입");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Player_DeathVolume,  "Ex.Runner.Player.DeathVolume",   "플레이어가 Kill Volume 진입");
