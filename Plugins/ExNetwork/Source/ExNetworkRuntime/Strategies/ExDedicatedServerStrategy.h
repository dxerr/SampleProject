// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/IExNetServerStrategy.h"

/**
 * FExDedicatedServerStrategy
 *
 * Dedicated Server 환경에서의 매치 전략. Phase 2에서는 전체 빈 골격.
 * Phase 4 이후에서 외부 서버 론칭 및 클라이언트 연결 로직이 추가된다.
 */
class FExDedicatedServerStrategy : public IExNetServerStrategy
{
public:

	explicit FExDedicatedServerStrategy();
	virtual ~FExDedicatedServerStrategy() override = default;

	/** IExNetServerStrategy 구현 */
	virtual EExServerType GetServerType() const override;
	virtual void CreateMatch(const FExMatchConfig& Config) override;
	virtual void JoinMatch(const FString& SessionId) override;
	virtual void StartGameSession(const FString& MapPath, UWorld* World) override;
	virtual void DestroyMatch() override;
	virtual void CancelMatch() override;

	virtual void BeginSearchPhase(const FExMatchConfig& Config, EExMatchState ExpectedState, TFunction<void(bool, const FString&)> OnSearchComplete) override;
	virtual void EndSearchPhase() override;

	virtual void BeginCreatePhase(const FExMatchConfig& Config, EExMatchState ExpectedState, TFunction<void(bool, const FString&)> OnCreateComplete) override;
	virtual void EndCreatePhase() override;

	virtual void BeginJoinPhase(const FExMatchConfig& Config, const FString& SessionId, EExMatchState ExpectedState, TFunction<void(bool, const FString&)> OnJoinComplete) override;
	virtual void EndJoinPhase() override;

	virtual void BeginWaitPhase(const FExMatchConfig& Config, bool bIsHostFlag, EExMatchState ExpectedState, TFunction<void(bool, const FString&)> OnReadyCallback) override;
	virtual void EndWaitPhase() override;

	virtual void ResetTransientState() override;
	virtual bool IsHost() const override;
	virtual FString GetConnectString() const override;
};
