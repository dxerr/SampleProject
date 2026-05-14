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
	}
}

void FExEOSLobbyProvider::CreateLobby(const FExMatchConfig& Config)
{
	if (!ensureMsgf(SessionInterface.IsValid(), TEXT("[ExEOSLobbyProvider] CreateLobby: SessionInterface 없음.")))
	{
		OnCreateComplete.Broadcast(false, TEXT("SessionInterface invalid"));
		return;
	}

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
	SessionSettings.bAllowJoinInProgress = false;
	SessionSettings.bIsLANMatch = false;
	SessionSettings.bUsesPresence = true;
	SessionSettings.bUseLobbiesIfAvailable = true;
	SessionSettings.bAllowInvites = false;
	SessionSettings.Set(FName("MatchMode"), Config.MatchMode, EOnlineDataAdvertisementType::ViaOnlineService);

	UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider] SessionSettings — NumPublicConnections=%d, bShouldAdvertise=%d, bUsesPresence=%d, bUseLobbiesIfAvailable=%d"),
		SessionSettings.NumPublicConnections,
		SessionSettings.bShouldAdvertise,
		SessionSettings.bUsesPresence,
		SessionSettings.bUseLobbiesIfAvailable);

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

	// 로그인 상태 확인
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
		SearchResults->MaxSearchResults,
		SearchResults->bIsLanQuery,
		*Config.MatchMode);

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
		ResultIndex,
		*Result.Session.GetSessionIdStr(),
		Result.PingInMs);

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

void FExEOSLobbyProvider::HandleCreateSessionComplete(FName SessionName, bool bSuccess)
{
	SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateCompleteHandle);

	if (bSuccess)
	{
		bInLobby = true;
		// 생성된 세션 정보 출력
		FNamedOnlineSession* Session = SessionInterface->GetNamedSession(SessionName);
		if (Session)
		{
			UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider] Lobby 생성 완료 — SessionId=%s, NumOpenPublicConnections=%d"),
				*Session->GetSessionIdStr(),
				Session->NumOpenPublicConnections);
		}
		else
		{
			UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider] Lobby 생성 완료 — SessionName=%s"), *SessionName.ToString());
		}
		OnCreateComplete.Broadcast(true, TEXT(""));
	}
	else
	{
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

		// 검색 결과 상세 출력
		if (SearchResults.IsValid())
		{
			for (int32 i = 0; i < SearchResults->SearchResults.Num(); i++)
			{
				const FOnlineSessionSearchResult& Result = SearchResults->SearchResults[i];
				FString MatchMode;
				Result.Session.SessionSettings.Get(FName("MatchMode"), MatchMode);
				UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider]   [%d] SessionId=%s, MatchMode=%s, OpenConnections=%d, Ping=%dms"),
					i,
					*Result.Session.GetSessionIdStr(),
					*MatchMode,
					Result.Session.NumOpenPublicConnections,
					Result.PingInMs);
			}
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

		// 접속 주소 확인
		FString ConnectInfo;
		if (SessionInterface->GetResolvedConnectString(SessionName, ConnectInfo))
		{
			UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider] Lobby 참가 완료 — SessionName=%s, ConnectString=%s"),
				*SessionName.ToString(), *ConnectInfo);
		}
		else
		{
			UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider] Lobby 참가 완료 — SessionName=%s (ConnectString 없음)"),
				*SessionName.ToString());
		}
		OnJoinComplete.Broadcast(true, TEXT(""));
	}
	else
	{
		UE_LOG(LogExNetwork, Warning, TEXT("[ExEOSLobbyProvider] Lobby 참가 실패 — Result=%d"), (int32)Result);
		OnJoinComplete.Broadcast(false, FString::Printf(TEXT("JoinSession failed: %d"), (int32)Result));
	}
}

void FExEOSLobbyProvider::HandleDestroySessionComplete(FName SessionName, bool bSuccess)
{
	SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroyCompleteHandle);

	bInLobby = false;
	UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider] Lobby 파괴 완료 — bSuccess=%d"), bSuccess);
	OnDestroyComplete.Broadcast(bSuccess);
}
