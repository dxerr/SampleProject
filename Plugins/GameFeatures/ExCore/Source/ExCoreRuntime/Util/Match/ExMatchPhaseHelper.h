#pragma once

#include "CoreMinimal.h"
#include "GameModes/ExGameStateBase.h"

/**
 * 매치 상태 확인을 위한 인라인 정적 헬퍼
 */
struct FExMatchPhaseHelper
{
	/**
	 * 매치가 현재 활성화 상태인지(Playing 이상) 확인합니다.
	 * 
	 * @param WorldContextObject 월드 컨텍스트
	 * @return 매치가 활성화 상태이면 true
	 */
	static inline bool IsMatchActive(const UObject* WorldContextObject)
	{
		if (WorldContextObject)
		{
			if (UWorld* World = WorldContextObject->GetWorld())
			{
				if (AExGameStateBase* GameState = World->GetGameState<AExGameStateBase>())
				{
					return GameState->IsMatchActive();
				}
			}
		}
		return false;
	}
};
