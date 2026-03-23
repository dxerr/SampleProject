// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExItemSystemTypes.generated.h"

/** 아이템 시스템 전용 로그 카테고리 */
EXCORERUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogExItemSystem, Log, All);

/**
 * 아이템 획득 방식 열거형
 * 향후 Interact, Proximity 등 확장 대비
 */
UENUM(BlueprintType)
enum class EExPickupMethod : uint8
{
	/** 충돌 즉시 획득 (기본, 러너 게임) */
	Overlap,

	/** 입력 키로 획득 (향후 확장) */
	Interact,

	/** 일정 거리 내 자동 획득 (자석 효과 등) */
	Proximity
};
