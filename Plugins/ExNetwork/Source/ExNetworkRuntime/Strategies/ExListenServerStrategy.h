// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/IExNetServerStrategy.h"
#include "Match/ExMatchTypes.h"
#include "Containers/Ticker.h"

class IExLobbyProvider;

/**
 * FExListenServerStrategy
 *
 * Listen Server 환경에서의 매치 생성/참가/시작 전략.
 * 호스트 PC가 서버 역할을 하며, 다른 플레이어는 EOS P2P로 연결된다.
 *
 * 외부에서 사용하는 주요 API:
 *   FindAndJoinOrCreate() — Quick Match 흐름 (검색 후 참가 또는 생성)
 *   StartGameSession()    — Lobby 파괴 후 ServerTravel 실행 (서버만 호출 가능)
 *   CancelMatch()         — 진행 중인 매칭 취소
 */
class FExListenServerStrategy : public IExNetServerStrategy
{
public:

	explicit FExListenServerStrategy();
	virtual ~FExListenServerStrategy() override;

	/** IExNetServerStrategy 구현 */
	virtual EExServerType GetServerType() const override;
	virtual void CreateMatch(const FExMatchConfig& Config) override;
	virtual void JoinMatch(const FString& SessionId) override;

	/**
	 * Lobby를 파괴하고 게임 맵으로 ServerTravel을 수행한다.
	 * 서버 권한에서만 동작. 클라이언트가 호출하면 경고 후 무시.
	 * UE 엔진이 연결된 모든 클라이언트를 자동으로 새 맵으로 이동시킨다.
	 *
	 * @param MapPath 전환할 맵 경로 (예: "/ExRunnerPlay/Map/L_ExRunnerTest")
	 * @param World   현재 World 참조
	 */
	virtual void StartGameSession(const FString& MapPath, UWorld* World) override;
	virtual void DestroyMatch() override;

	/** Quick Match: Lobby 검색 후 참가 또는 생성 자동 처리 */
	void FindAndJoinOrCreate(const FExMatchConfig& Config, TFunction<void(bool, const FString&)> OnComplete);

	/** 진행 중인 매칭 취소 */
	void CancelMatch();

	/** Lobby Provider 주입 */
	void SetLobbyProvider(TUniquePtr<IExLobbyProvider> InLobbyProvider);

	/** Lobby Provider 참조 */
	IExLobbyProvider* GetLobbyProvider() const { return LobbyProvider.Get(); }

private:

	void OnFindComplete(bool bSuccess, int32 ResultCount, FExMatchConfig Config, TFunction<void(bool, const FString&)> OnComplete);
	void OnCreateComplete(bool bSuccess, const FString& ErrorMessage, TFunction<void(bool, const FString&)> OnComplete);
	void OnJoinComplete(bool bSuccess, const FString& ErrorMessage, TFunction<void(bool, const FString&)> OnComplete);

	TUniquePtr<IExLobbyProvider> LobbyProvider;

	// 매치 대기 관련
	FTSTicker::FDelegateHandle WaitLobbyTickerHandle;
	float WaitLobbyElapsed = 0.f;
	FExMatchConfig CurrentWaitConfig;
	TFunction<void(bool, const FString&)> CachedOnComplete;

	// 검색 재시도 관련 (동시 접속 타이밍 문제 대응)
	// 검색 결과 0개일 때 즉시 생성하지 않고 MaxFindRetries회 재검색 후 생성
	static constexpr int32 MaxFindRetries = 3;
	static constexpr float FindRetryDelay = 2.0f; // 초
	int32 FindRetryCount = 0;

	void ClearWaitLobbyTicker();
	bool CheckLobbyWaitConditions_Host(float DeltaTime);
	bool CheckLobbyWaitConditions_Client(float DeltaTime);
};
