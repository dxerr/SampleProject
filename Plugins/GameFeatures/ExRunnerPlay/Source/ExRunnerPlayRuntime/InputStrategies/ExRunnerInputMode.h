// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExRunnerInputMode.generated.h"

/**
 * 러너 입력 모드 타입
 * None     : 입력 비활성화 (컷씬, 대기 등)
 * Manual   : 수동 입력 - 360도 자유 조향, 제한 없는 액션
 * AutoRun  : 자동 달리기 - 3레인 스냅 이동, 쿨다운 기반 액션
 */
UENUM(BlueprintType)
enum class EExRunnerInputMode : uint8
{
	None     UMETA(DisplayName = "없음 (입력 비활성화)"),
	Manual   UMETA(DisplayName = "수동 입력"),
	AutoRun  UMETA(DisplayName = "자동 달리기 (3레인)"),
};
