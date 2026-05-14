// Copyright ExFrameWork. All Rights Reserved.

#include "ExEOSLobbyProvider.h"
#include "Core/ExNetworkLog.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "Interfaces/OnlineIdentityInterface.h"

FExEOSLobbyProvider::FExEOSLobbyProvider(IOnlineSubsystem* InOSS)
	: OSS(InOSS)
{
	if (OSS)
	{
		SessionInterface = OSS->GetSessionInterface();
	}

	ensureMsgf(SessionInterface.IsValid(), TEXT("[ExEOSLobbyProvider] IOnlineSession 인터페이스를 가져올 수 없습니다."));
	UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider] 생성됨 — EOS IOnlineSession 기반."));
}

FExEOSLobbyProvider::~FExEOSLobbyProvider()
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateCompleteHandle);
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindCompleteHandle);
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinCompleteHandle);
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroyCompleteHandle);
		SessionInterface->ClearOnSessionParticipantJoinedDelegate_Handle(ParticipantsChangeHandle);
	}
}

void FExEOSLobbyProvider::CreateLobby(const FExMatchConfig& Config)
{
	if (!ensureMsgf(SessionInterface.IsValid(), TEXT("[ExEOSLobbyProvider] CreateLobby: SessionInterface 없음.")))
	{
		OnCreateComplete.Broadcast(false, TEXT("SessionInterface invalid"));
		return;
	}

	MaxPlayersCache = Config.MaxPlayers;

	// 로그인 상태 확인
	if (OSS)
	{
		IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
		if (Identity.IsValid())
		{
			ELoginStatus::Type LoginStatus = Identity->GetLoginStatus(0);
			TSharedPtr<const FUniqueNetId> UserId = Identity->GetUniquePlayerId(0);
			UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider] CreateLobby — LoginStatus=%d, UserId=%s"),
				(int32)LoginStatus,
				UserId.IsValid() ? *UserId->ToString() : TEXT("INVALID"));
		}
	}

	UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider] CreateLobby 시작 — MatchMode=%s, MaxPlayers=%d"),
		*Config.MatchMode, Config.MaxPlayers);

	FOnlineSessionSettings SessionSettings;
	SessionSettings.NumPublicConnections = Config.MaxPlayers;
	SessionSettings.bShouldAdvertise = true;
	SessionSettings.bAllowJoinInProgress = true;
	SessionSettings.bIsLANMatch = false;
	SessionSettings.bUsesPresence = true;
	SessionSettings.bUseLobbiesIfAvailable = true;
	SessionSettings.bAllowInvites = false;
	SessionSettings.Set(FName("MatchMode"), Config.MatchMode, EOnlineDataAdvertisementType::ViaOnlineService);

	UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider] SessionSettings — NumPublicConnections=%d, bShouldAdvertise=%d, bUsesPresence=%d"),
		SessionSettings.NumPublicConnections, SessionSettings.bShouldAdvertise, SessionSettings.bUsesPresence);

	// 참가자 변경 감지 등록 — 호스트가 상대방 입장을 감지하기 위해 CreateLobby 전에 바인딩
	ParticipantsChangeHandle = SessionInterface->AddOnSessionParticipantJoinedDelegate_Handle(
		FOnSessionParticipantJoinedDelegate::CreateRaw(this, &FExEOSLobbyProvider::HandleSessionParticipantJoined)
	);

	CreateCompleteHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateRaw(this, &FExEOSLobbyProvider::HandleCreateSessionComplete)
	);

	SessionInterface->CreateSession(0, ExMatchSessionName, SessionSettings);
}

void FExEOSLobbyProvider::FindLobbies(const FExMatchConfig& Config)
{
	if (!ensureMsgf(SessionInterface.IsValid(), TEXT("[ExEOSLobbyProvider] FindLobbies: SessionInterface 없음.")))
	{
		OnFindComplete.Broadcast(false, 0);
		return;
	}

	if (OSS)
	{
		IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
		if (Identity.IsValid())
		{
			ELoginStatus::Type LoginStatus = Identity->GetLoginStatus(0);
			TSharedPtr<const FUniqueNetId> UserId = Identity->GetUniquePlayerId(0);
			UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider] FindLobbies — LoginStatus=%d, UserId=%s"),
				(int32)LoginStatus,
				UserId.IsValid() ? *UserId->ToString() : TEXT("INVALID"));
		}
	}

	UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider] FindLobbies 시작 — MatchMode=%s"), *Config.MatchMode);

	SearchResults = MakeShared<FOnlineSessionSearch>();
	SearchResults->MaxSearchResults = 10;
	SearchResults->bIsLanQuery = false;
	SearchResults->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	SearchResults->QuerySettings.Set(FName("MatchMode"), Config.MatchMode, EOnlineComparisonOp::Equals);

	UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider] 검색 조건 — MaxSearchResults=%d, bIsLanQuery=%d, SEARCH_LOBBIES=true, MatchMode=%s"),
		SearchResults->MaxSearchResults, SearchResults->bIsLanQuery, *Config.MatchMode);

	FindCompleteHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateRaw(this, &FExEOSLobbyProvider::HandleFindSessionsComplete)
	);

	SessionInterface->FindSessions(0, SearchResults.ToSharedRef());
}

void FExEOSLobbyProvider::JoinLobby(int32 ResultIndex)
{
	if (!ensureMsgf(SessionInterface.IsValid(), TEXT("[ExEOSLobbyProvider] JoinLobby: SessionInterface 없음.")))
	{
		OnJoinComplete.Broadcast(false, TEXT("SessionInterface invalid"));
		return;
	}

	if (!SearchResults.IsValid() || !SearchResults->SearchResults.IsValidIndex(ResultIndex))
	{
		UE_LOG(LogExNetwork, Warning, TEXT("[ExEOSLobbyProvider] JoinLobby: 유효하지 않은 ResultIndex=%d"), ResultIndex);
		OnJoinComplete.Broadcast(false, TEXT("Invalid result index"));
		return;
	}

	const FOnlineSessionSearchResult& Result = SearchResults->SearchResults[ResultIndex];
	UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider] JoinLobby 시작 — ResultIndex=%d, SessionId=%s, Ping=%dms"),
		ResultIndex, *Result.Session.GetSessionIdStr(), Result.PingInMs);

	JoinCompleteHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateRaw(this, &FExEOSLobbyProvider::HandleJoinSessionComplete)
	);

	SessionInterface->JoinSession(0, ExMatchSessionName, Result);
}

void FExEOSLobbyProvider::DestroyLobby()
{
	if (!SessionInterface.IsValid())
	{
		OnDestroyComplete.Broadcast(false);
		return;
	}

	// 참가자 감지 해제
	SessionInterface->ClearOnSessionParticipantJoinedDelegate_Handle(ParticipantsChangeHandle);

	UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider] DestroyLobby 시작."));

	DestroyCompleteHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateRaw(this, &FExEOSLobbyProvider::HandleDestroySessionComplete)
	);

	SessionInterface->DestroySession(ExMatchSessionName);
}

bool FExEOSLobbyProvider::IsInLobby() const
{
	return bInLobby;
}

bool FExEOSLobbyProvider::HasLocalSession() const
{
	if (SessionInterface.IsValid())
	{
		return SessionInterface->GetNamedSession(ExMatchSessionName) != nullptr;
	}
	return false;
}

int32 FExEOSLobbyProvider::GetCurrentPlayerCount() const
{
	if (!SessionInterface.IsValid()) return 0;

	FNamedOnlineSession* Session = SessionInterface->GetNamedSession(ExMatchSessionName);
	if (!Session) return 0;

	// NumPublicConnections - NumOpenPublicConnections = 현재 참가 인원
	return Session->SessionSettings.NumPublicConnections - Session->NumOpenPublicConnections;
}

void FExEOSLobbyProvider::HandleCreateSessionComplete(FName SessionName, bool bSuccess)
{
	SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateCompleteHandle);

	if (bSuccess)
	{
		bInLobby = true;
		FNamedOnlineSession* Session = SessionInterface->GetNamedSession(SessionName);
		if (Session)
		{
			UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider] Lobby 생성 완료 — SessionId=%s, NumOpenPublicConnections=%d"),
				*Session->GetSessionIdStr(), Session->NumOpenPublicConnections);
		}
		OnCreateComplete.Broadcast(true, TEXT(""));
	}
	else
	{
		SessionInterface->ClearOnSessionParticipantJoinedDelegate_Handle(ParticipantsChangeHandle);
		UE_LOG(LogExNetwork, Warning, TEXT("[ExEOSLobbyProvider] Lobby 생성 실패 — SessionName=%s"), *SessionName.ToString());
		OnCreateComplete.Broadcast(false, TEXT("CreateSession failed"));
	}
}

void FExEOSLobbyProvider::HandleFindSessionsComplete(bool bSuccess)
{
	SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindCompleteHandle);

	const int32 ResultCount = SearchResults.IsValid() ? SearchResults->SearchResults.Num() : 0;

	if (bSuccess)
	{
		UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider] Lobby 검색 완료 — 결과 %d개"), ResultCount);

		for (int32 i = 0; i < ResultCount; i++)
		{
			const FOnlineSessionSearchResult& Result = SearchResults->SearchResults[i];
			FString MatchMode;
			Result.Session.SessionSettings.Get(FName("MatchMode"), MatchMode);
			UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider]   [%d] SessionId=%s, MatchMode=%s, OpenConnections=%d, Ping=%dms"),
				i, *Result.Session.GetSessionIdStr(), *MatchMode,
				Result.Session.NumOpenPublicConnections, Result.PingInMs);
		}
	}
	else
	{
		UE_LOG(LogExNetwork, Warning, TEXT("[ExEOSLobbyProvider] Lobby 검색 실패."));
	}

	OnFindComplete.Broadcast(bSuccess, ResultCount);
}

void FExEOSLobbyProvider::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinCompleteHandle);

	const bool bSuccess = (Result == EOnJoinSessionCompleteResult::Success);

	if (bSuccess)
	{
		bInLobby = true;
		FString ConnectInfo;
		SessionInterface->GetResolvedConnectString(SessionName, ConnectInfo);
		UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider] Lobby 참가 완료 — SessionName=%s, ConnectString=%s"),
			*SessionName.ToString(), *ConnectInfo);
		OnJoinComplete.Broadcast(true, TEXT(""));
	}
	else if (Result == EOnJoinSessionCompleteResult::AlreadyInSession)
	{
		// 이전 PIE 세션이 로컬에 남아있는 경우 — 기존 세션 파괴 후 재시도
		UE_LOG(LogExNetwork, Warning, TEXT("[ExEOSLobbyProvider] 이미 세션에 참가 중 — 기존 로컬 세션 파괴 후 재참가 시도."));

		// 기존 세션 파괴 후 재시도
		const TSharedPtr<FOnlineSessionSearch> CachedSearch = SearchResults;
		const int32 CachedResultIndex = 0; // 항상 첫 번째 결과

		FDelegateHandle CleanupHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateLambda(
				[this, CachedSearch, CachedResultIndex](FName DestroyedName, bool bDestroySuccess)
				{
					SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroyCompleteHandle);
					bInLobby = false;

					if (bDestroySuccess && CachedSearch.IsValid() && CachedSearch->SearchResults.IsValidIndex(CachedResultIndex))
					{
						UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider] 기존 세션 파괴 완료 — 재참가 시도."));
						JoinCompleteHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
							FOnJoinSessionCompleteDelegate::CreateRaw(this, &FExEOSLobbyProvider::HandleJoinSessionComplete)
						);
						SessionInterface->JoinSession(0, ExMatchSessionName, CachedSearch->SearchResults[CachedResultIndex]);
					}
					else
					{
						UE_LOG(LogExNetwork, Warning, TEXT("[ExEOSLobbyProvider] 기존 세션 파괴 후 재참가 불가."));
						OnJoinComplete.Broadcast(false, TEXT("AlreadyInSession cleanup failed"));
					}
				}
			)
		);
		DestroyCompleteHandle = CleanupHandle;
		SessionInterface->DestroySession(ExMatchSessionName);
	}
	else
	{
		const FString ResultStr = LexToString(Result);
		UE_LOG(LogExNetwork, Warning, TEXT("[ExEOSLobbyProvider] Lobby 참가 실패 — Result=%d (%s)"), (int32)Result, *ResultStr);
		OnJoinComplete.Broadcast(false, FString::Printf(TEXT("JoinSession failed: %s"), *ResultStr));
	}
}

void FExEOSLobbyProvider::HandleDestroySessionComplete(FName SessionName, bool bSuccess)
{
	SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroyCompleteHandle);
	bInLobby = false;
	UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider] Lobby 파괴 완료 — bSuccess=%d"), bSuccess);
	OnDestroyComplete.Broadcast(bSuccess);
}

void FExEOSLobbyProvider::HandleSessionParticipantJoined(FName SessionName, const FUniqueNetId& UniqueId)
{
	if (SessionName != ExMatchSessionName) return;

	const int32 CurrentCount = GetCurrentPlayerCount();
	UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider] 참가자 입장 — UserId=%s, 현재인원=%d/%d"),
		*UniqueId.ToString(), CurrentCount, MaxPlayersCache);

	// 정원이 채워지면 OnParticipantsFull 브로드캐스트 → 호스트에게 매칭 완료 알림
	if (CurrentCount >= MaxPlayersCache)
	{
		UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider] Lobby 정원 충족 (%d/%d) — OnParticipantsFull 브로드캐스트."),
			CurrentCount, MaxPlayersCache);
		OnParticipantsFull.Broadcast(CurrentCount);
	}
}
