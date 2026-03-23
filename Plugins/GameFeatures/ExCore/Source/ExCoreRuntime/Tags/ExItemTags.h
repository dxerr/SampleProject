// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * 아이템 시스템 전용 GameplayTag 정의
 * 아이템 획득 이벤트 및 버프 식별용 태그
 */

// ========== Item Pickup Event Tags ==========
EXCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ex_Item_PickedUp);
EXCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ex_Item_PickedUp_Score);
EXCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ex_Item_PickedUp_Buff);

// ========== Buff Tags ==========
EXCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ex_Buff_SpeedUp);
EXCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ex_Buff_Invincible);
EXCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ex_Buff_ScoreMultiplier);
EXCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ex_Buff_Magnet);
