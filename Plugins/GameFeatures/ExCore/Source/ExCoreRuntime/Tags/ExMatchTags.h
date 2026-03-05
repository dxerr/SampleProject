#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * 게임 매치 진행도(서버 권한 GameMode 관할)에 사용되는 네이티브 태그입니다.
 * 하드코딩된 문자열을 피하고 컴파일 타임 검증을 위해 ExCore에 선언합니다.
 */
namespace ExMatchTags
{
	EXCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Match_WaitingForPlayers);
	EXCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Match_Countdown);
	EXCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Match_Playing);
	EXCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Match_PostMatch);
}
