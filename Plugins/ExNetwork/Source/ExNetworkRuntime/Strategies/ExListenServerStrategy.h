// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/IExNetServerStrategy.h"
#include "Match/ExMatchTypes.h"

class IExLobbyProvider;

/**
 * FExListenServerStrategy
 *
 * Listen Server 환경에서의 매치 생성/참가 전략.
 * 호스트 PC가 서버 역할을 하며, 다른 플레이어는 EOS P2P로 연결된다.
 *
 * Phase 3: EOS Lobby 기반 Quick Match 구현
 *   FindAndJoinOrCreate() — Lobby 검색 후 참가 또는 생성
 * Phase 4: StartGameSession — ServerTravel 구현 예정
 */
class FExListenServerStrategy : public IExNetServerStrategy
{
public:

	explicit FExListenServerStrategy();
	virtual ~FExListenServerStrategy() override = default;

	/** IExNetServerStrategy 구현 */
	virtual EExServerType GetServerType() const override;
	virtual void CreateMatch(const FExMatchConfig& Config) override;
	virtual void JoinMatch(const FString& SessionId) override;
	virtual void StartGameSession(const FString& MapPath) override;
	virtual void DestroyMatch() override;

	/**
	 * Quick Match 핵심 흐름.
	 * Lobby 검색 후 결과에 따라 자동으로 참가 또는 생성한다.
	 * @param Config 매칭 설정
	 * @param OnComplete 매칭 완료 콜백 (성공 여부, 에러 메시지)
	 */
	void FindAndJoinOrCreate(const FExMatchConfig& Config, TFunction<void(bool, const FString&)> OnComplete);

	/** 현재 매칭 진행 취소 */
	void CancelMatch();

	/** Lobby Provider 주입 (UExOnlineSubsystem::Initialize에서 호출) */
	void SetLobbyProvider(TUniquePtr<IExLobbyProvider> InLobbyProvider);

	/** Lobby Provider 참조 (UExOnlineSubsystem에서 상태 확인용) */
	IExLobbyProvider* GetLobbyProvider() const { return LobbyProvider.Get(); }

private:

	void OnFindComplete(bool bSuccess, int32 ResultCount, FExMatchConfig Config, TFunction<void(bool, const FString&)> OnComplete);
	void OnCreateComplete(bool bSuccess, const FString& ErrorMessage, TFunction<void(bool, const FString&)> OnComplete);
	void OnJoinComplete(bool bSuccess, const FString& ErrorMessage, TFunction<void(bool, const FString&)> OnComplete);

	TUniquePtr<IExLobbyProvider> LobbyProvider;

	/** FindAndJoinOrCreate 진행 중 Config 소유 — Dangling Reference 방지 */
	FExMatchConfig PendingConfig;

};
