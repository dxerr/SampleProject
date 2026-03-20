// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * 음악(BGM) 시스템 전용 GameplayTag 정의
 * Quartz 비트/마디 이벤트를 게임플레이 이벤트 시스템으로 브로드캐스트할 때 사용
 */
namespace ExMusicTags
{
	// ========== 비트 이벤트 태그 ==========
	// Quartz 메트로놈 이벤트와 연동되는 태그
	EXCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Music_Beat);			// 매 비트 (4/4 기준 분기음표)
	EXCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Music_Bar);			// 매 마디 (4/4 기준 4비트 완료)
	EXCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Music_Beat_Strong);	// 강박 (1, 3번째 비트)

	// ========== 음악 Phase 태그 (2단계에서 활용) ==========
	// 인게임 내부 음악 분위기/강도 Phase
	EXCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Music_Phase_Warmup);	// 워밍업 (게임 시작 직후)
	EXCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Music_Phase_Running);	// 일반 러닝
	EXCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Music_Phase_Climax);	// 클라이맥스 (고난도 구간)
	EXCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Music_Phase_Cooldown);	// 쿨다운 (완화 구간)
}
