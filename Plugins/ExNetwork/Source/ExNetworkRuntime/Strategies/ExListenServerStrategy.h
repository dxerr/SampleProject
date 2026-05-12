// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/IExNetServerStrategy.h"

/**
 * FExListenServerStrategy
 *
 * Listen Server 환경에서의 매치 생성/참가 전략.
 * 호스트 PC가 서버 역할을 하며, 다른 플레이어는 P2P로 연결된다.
 *
 * Phase 2: 빈 골격 (로그만 출력)
 * Phase 3: EOS Lobby 생성/검색/참가 로직으로 채워질 예정
 * Phase 4: StartGameSession — ServerTravel 호출로 채워질 예정
 */
class FExListenServerStrategy : public IExNetServerStrategy
{
public:

	explicit FExListenServerStrategy();
	virtual ~FExListenServerStrategy() override = default;

	/** IExNetServerStrategy 구현 */
	virtual EExServerType GetServerType() const override;
	virtual void CreateMatch() override;
	virtual void JoinMatch(const FString& SessionId) override;
	virtual void StartGameSession(const FString& MapPath) override;
	virtual void DestroyMatch() override;
};
