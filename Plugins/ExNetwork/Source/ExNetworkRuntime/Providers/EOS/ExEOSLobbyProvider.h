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
 *
 * EOS Lobby 사용 조건:
 *   FOnlineSessionSettings::bUsesPresence = true  → EOS Lobby 경로
 *   FOnlineSessionSettings::bUsesPresence = false → EOS Session 경로
 *
 * 의존성:
 *   Initialize(IOnlineSubsystem*) 로 OSS 참조 주입 필요
 */
class FExEOSLobbyProvider : public IExLobbyProvider
{
public:

	explicit FExEOSLobbyProvider(IOnlineSubsystem* InOSS);
	virtual ~FExEOSLobbyProvider() override;

	/** IExLobbyProvider 구현 */
	virtual void CreateLobby(const FExMatchConfig& Config) override;
	virtual void FindLobbies(const FExMatchConfig& Config) override;
	virtual void JoinLobby(int32 ResultIndex) override;
	virtual void DestroyLobby() override;
	virtual bool IsInLobby() const override;

private:

	void HandleCreateSessionComplete(FName SessionName, bool bSuccess);
	void HandleFindSessionsComplete(bool bSuccess);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName SessionName, bool bSuccess);

	/** OSS 참조 (소유하지 않음 — UExOnlineSubsystem이 소유) */
	IOnlineSubsystem* OSS = nullptr;

	/** 세션 인터페이스 캐시 */
	IOnlineSessionPtr SessionInterface;

	/** FindLobbies 결과 저장 */
	TSharedPtr<FOnlineSessionSearch> SearchResults;

	/** 현재 Lobby 참가 여부 */
	bool bInLobby = false;

	/** 델리게이트 핸들 */
	FDelegateHandle CreateCompleteHandle;
	FDelegateHandle FindCompleteHandle;
	FDelegateHandle JoinCompleteHandle;
	FDelegateHandle DestroyCompleteHandle;

	/** 조인 실패 시 상세 로그 캡처기 */
	TSharedPtr<class FExEOSJoinLogCatcher> JoinLogCatcher;
};
