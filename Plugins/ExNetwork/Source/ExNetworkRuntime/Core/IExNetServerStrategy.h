// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ExNetworkTypes.h"

// FExMatchConfig 전방 선언 (헤더 의존성 최소화)
struct FExMatchConfig;

/**
 * IExNetServerStrategy
 *
 * Listen Server / Dedicated Server 두 모델을 동일한 인터페이스로 추상화한다.
 * UExOnlineSubsystem이 NetMode를 감지하여 적절한 구현체를 자동 선택한다.
 */
class IExNetServerStrategy
{
public:

	virtual ~IExNetServerStrategy() = default;

	virtual EExServerType GetServerType() const = 0;
	virtual void CreateMatch(const FExMatchConfig& Config) = 0;
	virtual void JoinMatch(const FString& SessionId) = 0;
	virtual void StartGameSession(const FString& MapPath) = 0;
	virtual void DestroyMatch() = 0;
};
