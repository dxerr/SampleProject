// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Providers/IExLobbyProvider.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineSessionInterface.h"

/**
 * FExEOSLobbyProvider
 *
 * IExLobbyProvider의 EOS 구현체.
 * UE 표준 IOnlineSession API를 통해 EOS Lobby를 생성/검색/참가한다.
 */
class FExEOSLobbyProvider : public IExLobbyProvider
{
public:

	explicit FExEOSLobbyProvider(IOnlineSubsystem* InOSS);
	virtual ~FExEOSLobbyProvider() override;

	virtual void CreateLobby(const FExMatchConfig& Config) override;
	virtual void FindLobbies(const FExMatchConfig& Config) override;
	virtual void JoinLobby(int32 ResultIndex) override;
	virtual void DestroyLobby() override;
	virtual bool IsInLobby() const override;
	virtual bool HasLocalSession() const override;
	virtual int32 GetCurrentPlayerCount() const override;
	virtual FString GetConnectString() const override;

private:

	void HandleCreateSessionComplete(FName SessionName, bool bSuccess);
	void HandleFindSessionsComplete(bool bSuccess);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName SessionName, bool bSuccess);

	/** 상대방이 Lobby에 입장했을 때 호출 (IOnlineSession::OnSessionParticipantJoined) */
	void HandleSessionParticipantJoined(FName SessionName, const FUniqueNetId& UniqueId);

	IOnlineSubsystem* OSS = nullptr;
	IOnlineSessionPtr SessionInterface;
	TSharedPtr<FOnlineSessionSearch> SearchResults;

	bool bInLobby = false;
	bool bIsDestroyed = false; // 소멸자 진입 여부 — EOS SDK 지연 콜백의 댕글링 포인터 크래시 방지

	/** 생성 시 설정한 MaxPlayers (참가자 수 비교용) */
	int32 MaxPlayersCache = 2;

	FDelegateHandle CreateCompleteHandle;
	FDelegateHandle FindCompleteHandle;
	FDelegateHandle JoinCompleteHandle;
	FDelegateHandle DestroyCompleteHandle;
	FDelegateHandle ParticipantsChangeHandle;

	/** JoinLobby 성공 시 저장 — Client가 호스트 서버에 접속할 주소 */
	FString CachedConnectString;
};
