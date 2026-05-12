// Copyright ExFrameWork. All Rights Reserved.

#include "ExItemTags.h"

/**
 * 아이템 시스템 GameplayTag 정의
 */

// ========== Item Pickup Event Tags ==========
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ex_Item_PickedUp, "Ex.Item.PickedUp", "아이템 획득 공통 이벤트");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ex_Item_PickedUp_Score, "Ex.Item.PickedUp.Score", "점수 아이템 획득 이벤트");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ex_Item_PickedUp_Buff, "Ex.Item.PickedUp.Buff", "버프 아이템 획득 이벤트");

// ========== Buff Tags ==========
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ex_Buff_SpeedUp,   "Ex.Buff.SpeedUp",   "속도 증가 버프");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ex_Buff_SpeedDown, "Ex.Buff.SpeedDown", "속도 감소 버프");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ex_Buff_Invincible, "Ex.Buff.Invincible", "무적 버프");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ex_Buff_ScoreMultiplier, "Ex.Buff.ScoreMultiplier", "점수 배율 버프");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ex_Buff_Magnet, "Ex.Buff.Magnet", "자석 효과 버프 (주변 아이템 자동 흡수)");
