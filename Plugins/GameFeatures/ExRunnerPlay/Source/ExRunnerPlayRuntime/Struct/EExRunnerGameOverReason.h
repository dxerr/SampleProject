// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EExRunnerGameOverReason.generated.h"

/**
 * EExRunnerGameOverReason
 * 러너 게임 종료(패배/승리) 원인 열거형
 * RuleManagerComponent가 GameState에 설정하고, ViewModel이 UI 분기에 사용함
 */
UENUM(BlueprintType)
enum class EExRunnerGameOverReason : uint8
{
	None,           // 게임 진행 중 (초기값)
	FallDeath,      // 낙하 사망 → 페이드아웃 + 재시작 팝업
	TimeUp,         // 시간 초과 → 결과 화면
	GoalReached,    // 목표 거리 달성 → 결과 화면 (성공)
};
