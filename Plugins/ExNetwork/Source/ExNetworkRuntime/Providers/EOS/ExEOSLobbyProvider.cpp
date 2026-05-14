// Copyright ExFrameWork. All Rights Reserved.

#include "ExEOSLobbyProvider.h"
#include "Core/ExNetworkLog.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "Misc/OutputDevice.h"

class FExEOSJoinLogCatcher : public FOutputDevice
{
public:
	FString LastError;
	
	FExEOSJoinLogCatcher()
	{
		GLog->AddOutputDevice(this);
	}
	
	virtual ~FExEOSJoinLogCatcher()
	{
		if (GLog)
		{
			GLog->RemoveOutputDevice(this);
		}
	}

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
	{
		if (Category == FName(TEXT("LogOnlineSession")) && Verbosity == ELogVerbosity::Warning)
		{
			FString Msg = V;
			if (Msg.Contains(TEXT("JoinLobby not successful")) || Msg.Contains(TEXT("JoinSession failed")))
			{
				LastError = Msg;
			}
		}
	}
};

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

	UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider] CreateLobby 시작 — MatchMode=%s, MaxPlayers=%d"),
		*Config.MatchMode, Config.MaxPlayers);

	FOnlineSessionSettings SessionSettings;
	SessionSettings.NumPublicConnections = Config.MaxPlayers;
	SessionSettings.bShouldAdvertise = true;       // 검색 가능하도록 공개
	SessionSettings.bAllowJoinInProgress = false;  // 게임 시작 후 참가 차단
	SessionSettings.bIsLANMatch = false;           // 인터넷 매칭
	SessionSettings.bUsesPresence = true;          // EOS Lobby 경로 사용 (false면 EOS Session)
	SessionSettings.bUseLobbiesIfAvailable = true; // Lobby API 사용 명시
	SessionSettings.bAllowInvites = false;         // Phase 6+에서 활성화
	SessionSettings.Set(FName("MatchMode"), Config.MatchMode, EOnlineDataAdvertisementType::ViaOnlineService);

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

	UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider] FindLobbies 시작 — MatchMode=%s"), *Config.MatchMode);

	SearchResults = MakeShared<FOnlineSessionSearch>();
	SearchResults->MaxSearchResults = 10;
	SearchResults->bIsLanQuery = false;
	SearchResults->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	SearchResults->QuerySettings.Set(SEARCH_MINSLOTSAVAILABLE, 1, EOnlineComparisonOp::GreaterThanEquals);
	SearchResults->QuerySettings.Set(FName("MatchMode"), Config.MatchMode, EOnlineComparisonOp::Equals);

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

	UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider] JoinLobby 시작 — ResultIndex=%d"), ResultIndex);

	JoinLogCatcher = MakeShared<FExEOSJoinLogCatcher>();

	JoinCompleteHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateRaw(this, &FExEOSLobbyProvider::HandleJoinSessionComplete)
	);

	SessionInterface->JoinSession(0, ExMatchSessionName, SearchResults->SearchResults[ResultIndex]);
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
		UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider] Lobby 생성 완료 — SessionName=%s"), *SessionName.ToString());
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

	FString DetailedError = FString::Printf(TEXT("JoinSession failed: %d"), (int32)Result);
	if (JoinLogCatcher.IsValid() && !JoinLogCatcher->LastError.IsEmpty())
	{
		DetailedError = JoinLogCatcher->LastError;
	}
	JoinLogCatcher.Reset();

	const bool bSuccess = (Result == EOnJoinSessionCompleteResult::Success);

	if (bSuccess)
	{
		bInLobby = true;
		UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider] Lobby 참가 완료 — SessionName=%s"), *SessionName.ToString());
		OnJoinComplete.Broadcast(true, TEXT(""));
	}
	else
	{
		UE_LOG(LogExNetwork, Warning, TEXT("[ExEOSLobbyProvider] Lobby 참가 실패 — 세부 사유: %s"), *DetailedError);
		OnJoinComplete.Broadcast(false, DetailedError);
	}
}

void FExEOSLobbyProvider::HandleDestroySessionComplete(FName SessionName, bool bSuccess)
{
	SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroyCompleteHandle);

	bInLobby = false;
	UE_LOG(LogExNetwork, Log, TEXT("[ExEOSLobbyProvider] Lobby 파괴 완료 — bSuccess=%d"), bSuccess);
	OnDestroyComplete.Broadcast(bSuccess);
}
