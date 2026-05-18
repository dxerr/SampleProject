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

	/** Quick Match: Lobby 검색 후 참가 또는 생성 자동 처리 (Phase B 이후 deprecated, 내부 호출용으로 분리됨) */
	void FindAndJoinOrCreate(const FExMatchConfig& Config, TFunction<void(bool, const FString&)> OnComplete);

	/** 진행 중인 매칭 취소 */
	virtual void CancelMatch() override;

	/** Lobby Provider 주입 */
	void SetLobbyProvider(TSharedPtr<IExLobbyProvider> InLobbyProvider);

	/** Online Subsystem 주입 */
	void SetOnlineSubsystem(class IOnlineSubsystem* InOSS) { OSSInstance = InOSS; }

	/** Lobby Provider 참조 */
	IExLobbyProvider* GetLobbyProvider() const { return LobbyProvider.Get(); }

	/** 현재 인스턴스가 Host로 동작 중인지 여부 반환 */
	virtual bool IsHost() const override { return bIsHost; }

	/** Client가 서버에 접속하기 위한 ConnectString 반환 */
	virtual FString GetConnectString() const override { return CachedConnectString; }

	// --- FSM 전이용 Phase 메서드 군 ---

	virtual void BeginSearchPhase(const FExMatchConfig& Config, EExMatchState ExpectedState, TFunction<void(bool, const FString&)> OnSearchComplete) override;
	virtual void EndSearchPhase() override;

	virtual void BeginCreatePhase(const FExMatchConfig& Config, EExMatchState ExpectedState, TFunction<void(bool, const FString&)> OnCreateComplete) override;
	virtual void EndCreatePhase() override;

	virtual void BeginJoinPhase(const FExMatchConfig& Config, const FString& SessionId, EExMatchState ExpectedState, TFunction<void(bool, const FString&)> OnJoinComplete) override;
	virtual void EndJoinPhase() override;

	virtual void BeginWaitPhase(const FExMatchConfig& Config, bool bIsHostFlag, EExMatchState ExpectedState, TFunction<void(bool, const FString&)> OnReadyCallback) override;
	virtual void EndWaitPhase() override;

	virtual void ResetTransientState() override;

private:

	TSharedPtr<IExLobbyProvider> LobbyProvider;
	class IOnlineSubsystem* OSSInstance = nullptr;

	// 매치 대기 관련
	FTSTicker::FDelegateHandle WaitLobbyTickerHandle;
	FTSTicker::FDelegateHandle FindRetryTickerHandle;
	double WaitStartTime = 0.0;   // FPlatformTime::Seconds() 절대 시간 기반
	FExMatchConfig CurrentWaitConfig;
	TFunction<void(bool, const FString&)> CachedOnComplete;

	/** UpdateSession(MATCH_STARTED=1) 핸들 — 소멸자에서 해제하여 ServerTravel 후 댕글링 콜백 크래시 방지 */
	FDelegateHandle UpdateSessionHandle;

	/** 소멸자 진입 여부 — UpdateSession 지연 콜백의 댕글링 this 접근 크래시 방지 */
	bool bIsDestroyed = false;

	/** 현재 인스턴스가 Host로 동작하는지 여부 */
	bool bIsHost = false;

	// 검색 재시도 관련
	// MaxWaitForPlayersSeconds 동안 FindRetryDelay 간격으로 재검색
	// 타임아웃 후에도 Lobby 없으면 그때 생성
	static constexpr float FindRetryDelay = 2.0f;
	int32 FindRetryCount = 0;

	/** JoinLobby 성공 시 저장 — MATCH_STARTED 감지 후 ClientTravel에 사용 */
	FString CachedConnectString;

	FTSTicker::FDelegateHandle SearchPhaseTickerHandle;
	FTSTicker::FDelegateHandle CreatePhaseTickerHandle;
	FTSTicker::FDelegateHandle JoinPhaseTickerHandle;

	void ClearWaitLobbyTicker();
	bool CheckLobbyWaitConditions_Host(float DeltaTime);
	bool CheckLobbyWaitConditions_Client(float DeltaTime);
};
