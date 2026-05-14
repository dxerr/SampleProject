// Copyright ExFrameWork. All Rights Reserved.

#include "ExListenServerStrategy.h"
#include "Core/ExNetworkLog.h"
#include "Providers/IExLobbyProvider.h"
#include "Engine/World.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "GameFramework/PlayerController.h"

FExListenServerStrategy::FExListenServerStrategy()
{
	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] 생성됨 — Listen Server 모드."));
}

EExServerType FExListenServerStrategy::GetServerType() const
{
	return EExServerType::ListenServer;
}

void FExListenServerStrategy::SetLobbyProvider(TUniquePtr<IExLobbyProvider> InLobbyProvider)
{
	LobbyProvider = MoveTemp(InLobbyProvider);
	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] LobbyProvider 주입 완료."));
}

void FExListenServerStrategy::CreateMatch(const FExMatchConfig& Config)
{
	if (!ensureMsgf(LobbyProvider, TEXT("[ExListenServerStrategy] CreateMatch: LobbyProvider 없음.")))
	{
		return;
	}
	LobbyProvider->CreateLobby(Config);
}

void FExListenServerStrategy::JoinMatch(const FString& SessionId)
{
	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] JoinMatch — FindAndJoinOrCreate 사용 권장."));
}

void FExListenServerStrategy::StartGameSession(const FString& MapPath, UWorld* World)
{
	if (!ensureMsgf(World, TEXT("[ExListenServerStrategy] StartGameSession: World 없음.")))
	{
		return;
	}

	// 클라이언트는 ClientTravel 수행
	if (World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] StartGameSession: 클라이언트는 호스트로 ClientTravel을 시도합니다."));
		IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
		if (OSS && OSS->GetSessionInterface().IsValid())
		{
			FString ConnectInfo;
			if (OSS->GetSessionInterface()->GetResolvedConnectString(ExMatchSessionName, ConnectInfo))
			{
				if (APlayerController* PC = World->GetFirstPlayerController())
				{
					UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] ClientTravel -> %s"), *ConnectInfo);
					PC->ClientTravel(ConnectInfo, TRAVEL_Absolute);
				}
			}
			else
			{
				UE_LOG(LogExNetwork, Warning, TEXT("[ExListenServerStrategy] GetResolvedConnectString 실패. 호스트의 연결 정보를 가져올 수 없습니다."));
			}
		}
		return;
	}

	if (!ensureMsgf(!MapPath.IsEmpty(), TEXT("[ExListenServerStrategy] StartGameSession: MapPath가 비어있습니다.")))
	{
		return;
	}

	// Lobby 명시적 파괴 — 새 플레이어 참가 차단 후 게임 전환
	if (LobbyProvider && LobbyProvider->IsInLobby())
	{
		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] StartGameSession: Lobby 파괴 후 ServerTravel 진행."));
		LobbyProvider->DestroyLobby();
	}

	// ServerTravel — ?listen 옵션으로 클라이언트가 새 맵에서도 접속 유지
	// UE 엔진이 연결된 모든 클라이언트를 자동으로 새 맵으로 이동시킨다.
	const FString TravelURL = MapPath + TEXT("?listen");
	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] ServerTravel 실행 — URL=%s"), *TravelURL);

	World->ServerTravel(TravelURL);
}

void FExListenServerStrategy::DestroyMatch()
{
	if (!LobbyProvider)
	{
		return;
	}
	LobbyProvider->DestroyLobby();
}

void FExListenServerStrategy::FindAndJoinOrCreate(const FExMatchConfig& Config, TFunction<void(bool, const FString&)> OnComplete)
{
	if (!ensureMsgf(LobbyProvider, TEXT("[ExListenServerStrategy] FindAndJoinOrCreate: LobbyProvider 없음.")))
	{
		OnComplete(false, TEXT("LobbyProvider not set"));
		return;
	}

	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Quick Match 시작 — Lobby 검색 중..."));

	LobbyProvider->OnFindComplete.AddLambda(
		[this, Config, OnComplete](bool bSuccess, int32 ResultCount)
		{
			FExListenServerStrategy* SafeThis = this;
			FExMatchConfig SafeConfig = Config;
			TFunction<void(bool, const FString&)> SafeOnComplete = OnComplete;
			SafeThis->LobbyProvider->OnFindComplete.Clear();
			SafeThis->CurrentWaitConfig = SafeConfig;
			SafeThis->OnFindComplete(bSuccess, ResultCount, SafeConfig, SafeOnComplete);
		}
	);

	LobbyProvider->FindLobbies(Config);
}

void FExListenServerStrategy::OnFindComplete(bool bSuccess, int32 ResultCount, FExMatchConfig Config, TFunction<void(bool, const FString&)> OnComplete)
{
	if (bSuccess && ResultCount > 0)
	{
		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Lobby %d개 발견 — 첫 번째 Lobby 참가 시도."), ResultCount);

		LobbyProvider->OnJoinComplete.AddLambda(
			[this, OnComplete](bool bJoinSuccess, const FString& ErrorMessage)
			{
				FExListenServerStrategy* SafeThis = this;
				TFunction<void(bool, const FString&)> SafeOnComplete = OnComplete;
				SafeThis->LobbyProvider->OnJoinComplete.Clear();
				SafeThis->OnJoinComplete(bJoinSuccess, ErrorMessage, SafeOnComplete);
			}
		);

		LobbyProvider->JoinLobby(0);
	}
	else
	{
		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] 빈 Lobby 없음 — 새 Lobby 생성 중..."));

		LobbyProvider->OnCreateComplete.AddLambda(
			[this, OnComplete](bool bCreateSuccess, const FString& ErrorMessage)
			{
				FExListenServerStrategy* SafeThis = this;
				TFunction<void(bool, const FString&)> SafeOnComplete = OnComplete;
				SafeThis->LobbyProvider->OnCreateComplete.Clear();
				SafeThis->OnCreateComplete(bCreateSuccess, ErrorMessage, SafeOnComplete);
			}
		);

		LobbyProvider->CreateLobby(Config);
	}
}

void FExListenServerStrategy::OnCreateComplete(bool bSuccess, const FString& ErrorMessage, TFunction<void(bool, const FString&)> OnComplete)
{
	if (bSuccess)
	{
		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Lobby 생성 성공 — 상대 플레이어 대기 중."));
		WaitLobbyElapsed = 0.f;
		CachedOnComplete = OnComplete;
		WaitLobbyTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FExListenServerStrategy::CheckLobbyWaitConditions_Host), 1.0f);
	}
	else
	{
		UE_LOG(LogExNetwork, Warning, TEXT("[ExListenServerStrategy] Lobby 생성 실패 — %s"), *ErrorMessage);
		OnComplete(bSuccess, ErrorMessage);
	}
}

void FExListenServerStrategy::OnJoinComplete(bool bSuccess, const FString& ErrorMessage, TFunction<void(bool, const FString&)> OnComplete)
{
	if (bSuccess)
	{
		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Lobby 참가 성공 — 매치 시작 대기 중."));
		WaitLobbyElapsed = 0.f;
		CachedOnComplete = OnComplete;
		WaitLobbyTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FExListenServerStrategy::CheckLobbyWaitConditions_Client), 1.0f);
	}
	else
	{
		UE_LOG(LogExNetwork, Warning, TEXT("[ExListenServerStrategy] Lobby 참가 실패 — %s"), *ErrorMessage);
		OnComplete(bSuccess, ErrorMessage);
	}
}

void FExListenServerStrategy::CancelMatch()
{
	ClearWaitLobbyTicker();
	if (LobbyProvider && LobbyProvider->IsInLobby())
	{
		LobbyProvider->DestroyLobby();
	}
	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] 매칭 취소."));
}

void FExListenServerStrategy::ClearWaitLobbyTicker()
{
	if (WaitLobbyTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(WaitLobbyTickerHandle);
		WaitLobbyTickerHandle.Reset();
	}
}

bool FExListenServerStrategy::CheckLobbyWaitConditions_Host(float DeltaTime)
{
	WaitLobbyElapsed += DeltaTime;
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
	if (!OSS || !OSS->GetSessionInterface().IsValid())
	{
		return true;
	}

	FNamedOnlineSession* Session = OSS->GetSessionInterface()->GetNamedSession(ExMatchSessionName);
	if (!Session)
	{
		return true;
	}

	int32 CurrentPlayers = Session->RegisteredPlayers.Num();
	if (CurrentWaitConfig.bIsSinglePlay || CurrentPlayers >= CurrentWaitConfig.ExpectedPlayerCount)
	{
		FOnlineSessionSettings* Settings = OSS->GetSessionInterface()->GetSessionSettings(ExMatchSessionName);
		if (Settings)
		{
			Settings->Set(FName("MATCH_STARTED"), 1, EOnlineDataAdvertisementType::ViaOnlineService);
			OSS->GetSessionInterface()->UpdateSession(ExMatchSessionName, *Settings, true);
		}

		ClearWaitLobbyTicker();
		if (CachedOnComplete)
		{
			CachedOnComplete(true, TEXT(""));
			CachedOnComplete = nullptr;
		}
		return false; 
	}

	if (WaitLobbyElapsed >= CurrentWaitConfig.MaxWaitForPlayersSeconds)
	{
		ClearWaitLobbyTicker();
		if (LobbyProvider)
		{
			LobbyProvider->DestroyLobby();
		}
		if (CachedOnComplete)
		{
			CachedOnComplete(false, TEXT("Timeout"));
			CachedOnComplete = nullptr;
		}
		return false;
	}

	return true; 
}

bool FExListenServerStrategy::CheckLobbyWaitConditions_Client(float DeltaTime)
{
	WaitLobbyElapsed += DeltaTime;
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
	if (!OSS || !OSS->GetSessionInterface().IsValid())
	{
		return true;
	}

	FOnlineSessionSettings* Settings = OSS->GetSessionInterface()->GetSessionSettings(ExMatchSessionName);
	if (Settings)
	{
		int32 MatchStarted = 0;
		if (Settings->Get(FName("MATCH_STARTED"), MatchStarted) && MatchStarted == 1)
		{
			ClearWaitLobbyTicker();
			if (CachedOnComplete)
			{
				CachedOnComplete(true, TEXT(""));
				CachedOnComplete = nullptr;
			}
			return false; 
		}
	}

	if (WaitLobbyElapsed >= CurrentWaitConfig.MaxWaitForPlayersSeconds)
	{
		ClearWaitLobbyTicker();
		if (LobbyProvider)
		{
			LobbyProvider->DestroyLobby();
		}
		if (CachedOnComplete)
		{
			CachedOnComplete(false, TEXT("Timeout"));
			CachedOnComplete = nullptr;
		}
		return false;
	}

	return true; 
}
