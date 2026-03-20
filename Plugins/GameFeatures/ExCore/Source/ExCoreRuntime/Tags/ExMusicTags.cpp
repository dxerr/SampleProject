// Copyright ExFrameWork. All Rights Reserved.

#include "Tags/ExMusicTags.h"

namespace ExMusicTags
{
	// 비트 이벤트 태그
	UE_DEFINE_GAMEPLAY_TAG(Music_Beat, "Ex.Music.Beat");
	UE_DEFINE_GAMEPLAY_TAG(Music_Bar, "Ex.Music.Bar");
	UE_DEFINE_GAMEPLAY_TAG(Music_Beat_Strong, "Ex.Music.Beat.Strong");

	// 음악 Phase 태그
	UE_DEFINE_GAMEPLAY_TAG(Music_Phase_Warmup, "Ex.Music.Phase.Warmup");
	UE_DEFINE_GAMEPLAY_TAG(Music_Phase_Running, "Ex.Music.Phase.Running");
	UE_DEFINE_GAMEPLAY_TAG(Music_Phase_Climax, "Ex.Music.Phase.Climax");
	UE_DEFINE_GAMEPLAY_TAG(Music_Phase_Cooldown, "Ex.Music.Phase.Cooldown");
}
