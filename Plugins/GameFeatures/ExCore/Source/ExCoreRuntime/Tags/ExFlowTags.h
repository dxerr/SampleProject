#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * ExCore에서 전역적으로 사용하는 앱 레벨 Flow 상태용 네이티브 게임플레이 태그
 * 하드코딩된 문자열 검사를 피하기 위해 C++ 네이티브 태그로 등록하여 사용합니다.
 */
namespace ExFlowTags
{
	EXCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Flow_Boot);
	EXCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Flow_Auth_IDP);
	EXCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Flow_Lobby);
	EXCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Flow_InGame);
}
