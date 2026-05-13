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
	virtual void StartGameSession(const FString& MapPath) override;
	virtual void DestroyMatch() override;
};
