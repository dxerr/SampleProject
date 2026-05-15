// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ExNetworkTypes.h"
#include "Match/ExMatchTypes.h"

struct FExMatchConfig;
class UWorld;

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

	/**
	 * 게임 맵으로 전환한다.
	 * @param MapPath 전환할 맵 경로
	 * @param World   현재 World 참조 (ServerTravel에 필요)
	 */
	virtual void StartGameSession(const FString& MapPath, UWorld* World) = 0;
	virtual void DestroyMatch() = 0;
	virtual void CancelMatch() = 0;

	// --- FSM 전이용 Phase 메서드 군 ---

	virtual void BeginSearchPhase(const FExMatchConfig& Config, EExMatchState ExpectedState, TFunction<void(bool, const FString&)> OnSearchComplete) = 0;
	virtual void EndSearchPhase() = 0;

	virtual void BeginCreatePhase(const FExMatchConfig& Config, EExMatchState ExpectedState, TFunction<void(bool, const FString&)> OnCreateComplete) = 0;
	virtual void EndCreatePhase() = 0;

	// Note: SessionId instead of FString as per implementation details (SessionId is the connect string or specific index, we will leave it as FString)
	virtual void BeginJoinPhase(const FExMatchConfig& Config, const FString& SessionId, EExMatchState ExpectedState, TFunction<void(bool, const FString&)> OnJoinComplete) = 0;
	virtual void EndJoinPhase() = 0;

	virtual void BeginWaitPhase(const FExMatchConfig& Config, bool bIsHostFlag, EExMatchState ExpectedState, TFunction<void(bool, const FString&)> OnReadyCallback) = 0;
	virtual void EndWaitPhase() = 0;

	virtual void ResetTransientState() = 0;
	virtual bool IsHost() const = 0;
	virtual FString GetConnectString() const = 0;
};
