// Copyright ExFrameWork. All Rights Reserved.

#include "ExListenServerStrategy.h"
#include "Core/ExNetworkLog.h"
#include "Providers/IExLobbyProvider.h"
#include "Engine/World.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"

FExListenServerStrategy::FExListenServerStrategy()
{
	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] 생성됨 — Listen Server 모드."));
}

FExListenServerStrategy::~FExListenServerStrategy()
{
	ClearWaitLobbyTicker();
	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] 소멸됨 — Ticker 핸들 해제."));
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

	if (World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogExNetwork, Warning, TEXT("[ExListenServerStrategy] StartGameSession: 클라이언트는 호출 불가."));
		return;
	}

	if (!ensureMsgf(!MapPath.IsEmpty(), TEXT("[ExListenServerStrategy] StartGameSession: MapPath가 비어있습니다.")))
	{
		return;
	}

	if (LobbyProvider && LobbyProvider->IsInLobby())
	{
		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] StartGameSession: Lobby 파괴 후 ServerTravel 진행."));
		LobbyProvider->DestroyLobby();
	}

	const FString TravelURL = MapPath + TEXT("?listen");
	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] ServerTravel 실행 — URL=%s"), *TravelURL);
	World->ServerTravel(TravelURL);
}

void FExListenServerStrategy::DestroyMatch()
{
	if (!LobbyProvider) return;
	LobbyProvider->DestroyLobby();
}

void FExListenServerStrategy::FindAndJoinOrCreate(const FExMatchConfig& Config, TFunction<void(bool, const FString&)> OnComplete)
{
	if (!ensureMsgf(LobbyProvider, TEXT("[ExListenServerStrategy] FindAndJoinOrCreate: LobbyProvider 없음.")))
	{
		OnComplete(false, TEXT("LobbyProvider not set"));
		return;
	}

	if (LobbyProvider->HasLocalSession())
	{
		UE_LOG(LogExNetwork, Warning, TEXT("[ExListenServerStrategy] 이전 Session이 남아있습니다. 파괴 후 매칭을 재진행합니다."));
		LobbyProvider->OnDestroyComplete.Clear();
		LobbyProvider->OnDestroyComplete.AddLambda(
			[this, Config, OnComplete](bool bDestroySuccess)
			{
				LobbyProvider->OnDestroyComplete.Clear();
				this->FindAndJoinOrCreate(Config, OnComplete);
			}
		);
		LobbyProvider->DestroyLobby();
		return;
	}

	LobbyProvider->OnFindComplete.Clear();
	LobbyProvider->OnCreateComplete.Clear();
	LobbyProvider->OnJoinComplete.Clear();

	FindRetryCount = 0;
	WaitStartTime = FPlatformTime::Seconds(); // 매칭 시작 시점 캡처

	if (Config.bIsSinglePlay)
	{
		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Single Play 모드 — Lobby 즉시 생성."));
		CurrentWaitConfig = Config;

		LobbyProvider->OnCreateComplete.AddLambda(
			[this, OnComplete](bool bCreateSuccess, const FString& ErrorMessage)
			{
				LobbyProvider->OnCreateComplete.Clear();
				OnCreateComplete(bCreateSuccess, ErrorMessage, OnComplete);
			}
		);
		LobbyProvider->CreateLobby(Config);
		return;
	}

	// Multi Play: 검색 먼저
	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Quick Match 시작 — Lobby 검색 중..."));
	CurrentWaitConfig = Config;

	LobbyProvider->OnFindComplete.AddLambda(
		[this, OnComplete](bool bSuccess, int32 ResultCount)
		{
			LobbyProvider->OnFindComplete.Clear();
			OnFindComplete(bSuccess, ResultCount, OnComplete);
		}
	);
	LobbyProvider->FindLobbies(Config);
}

void FExListenServerStrategy::OnFindComplete(bool bSuccess, int32 ResultCount, TFunction<void(bool, const FString&)> OnComplete)
{
	if (bSuccess && ResultCount > 0)
	{
		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Lobby %d개 발견 — 첫 번째 Lobby 참가 시도."), ResultCount);
		FindRetryCount = 0;

		LobbyProvider->OnJoinComplete.Clear();
		LobbyProvider->OnJoinComplete.AddLambda(
			[this, OnComplete](bool bJoinSuccess, const FString& ErrorMessage)
			{
				LobbyProvider->OnJoinComplete.Clear();
				OnJoinComplete(bJoinSuccess, ErrorMessage, OnComplete);
			}
		);
		LobbyProvider->JoinLobby(0);
	}
	else
	{
		// 검색 결과 없음
		const double Elapsed = FPlatformTime::Seconds() - WaitStartTime;

		if (Elapsed >= CurrentWaitConfig.MaxWaitForPlayersSeconds)
		{
			// 전체 대기 시간 소진 → 이 PC가 Host로 Lobby 생성
			FindRetryCount = 0;
			UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] %.1f초 대기 후 Lobby 없음 — 이 PC가 Host로 Lobby 생성."), Elapsed);

			LobbyProvider->OnCreateComplete.Clear();
			LobbyProvider->OnCreateComplete.AddLambda(
				[this, OnComplete](bool bCreateSuccess, const FString& ErrorMessage)
				{
					LobbyProvider->OnCreateComplete.Clear();
					OnCreateComplete(bCreateSuccess, ErrorMessage, OnComplete);
				}
			);
			LobbyProvider->CreateLobby(CurrentWaitConfig);
		}
		else
		{
			// 아직 대기 시간 남음 → 계속 재검색
			FindRetryCount++;
			UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Lobby 없음 — %.1f초 후 재검색 시도 (%d회). 경과=%.1f/%.1f초"),
				FindRetryDelay, FindRetryCount,
				Elapsed, CurrentWaitConfig.MaxWaitForPlayersSeconds);

			// 재시도 Ticker 등록 — 핸들을 저장하여 종료/취소 시 안전하게 해제 가능하게 함
			FindRetryTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateLambda([this, OnComplete](float) -> bool
				{
					FindRetryTickerHandle.Reset(); // 실행됐으므로 핸들 초기화
					LobbyProvider->OnFindComplete.Clear();
					LobbyProvider->OnFindComplete.AddLambda(
						[this, OnComplete](bool bS, int32 RC)
						{
							LobbyProvider->OnFindComplete.Clear();
							OnFindComplete(bS, RC, OnComplete);
						}
					);
					LobbyProvider->FindLobbies(CurrentWaitConfig);
					return false;
				}),
				FindRetryDelay
			);
		}
	}
}

void FExListenServerStrategy::OnCreateComplete(bool bSuccess, const FString& ErrorMessage, TFunction<void(bool, const FString&)> OnComplete)
{
	if (bSuccess)
	{
		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Lobby 생성 성공 — 상대 플레이어 대기 중."));
		WaitStartTime = FPlatformTime::Seconds(); // 절대 시간 캡처
		CachedOnComplete = OnComplete;
		// 1초 간격 Ticker
		WaitLobbyTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateRaw(this, &FExListenServerStrategy::CheckLobbyWaitConditions_Host), 1.0f);
	}
	else
	{
		UE_LOG(LogExNetwork, Warning, TEXT("[ExListenServerStrategy] Lobby 생성 실패 — %s"), *ErrorMessage);
		OnComplete(false, ErrorMessage);
	}
}

void FExListenServerStrategy::OnJoinComplete(bool bSuccess, const FString& ErrorMessage, TFunction<void(bool, const FString&)> OnComplete)
{
	if (bSuccess)
	{
		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Lobby 참가 성공 — 매치 시작 대기 중."));
		WaitStartTime = FPlatformTime::Seconds(); // 절대 시간 캡처
		CachedOnComplete = OnComplete;
		WaitLobbyTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateRaw(this, &FExListenServerStrategy::CheckLobbyWaitConditions_Client), 1.0f);
	}
	else
	{
		UE_LOG(LogExNetwork, Warning, TEXT("[ExListenServerStrategy] Lobby 참가 실패 — %s"), *ErrorMessage);
		OnComplete(false, ErrorMessage);
	}
}

void FExListenServerStrategy::CancelMatch()
{
	ClearWaitLobbyTicker();
	if (LobbyProvider)
	{
		LobbyProvider->OnFindComplete.Clear();
		LobbyProvider->OnCreateComplete.Clear();
		LobbyProvider->OnJoinComplete.Clear();

		if (LobbyProvider->IsInLobby())
		{
			LobbyProvider->DestroyLobby();
		}
	}
	FindRetryCount = 0;
	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] 매칭 취소."));
}

void FExListenServerStrategy::ClearWaitLobbyTicker()
{
	// 대기 Ticker 해제
	if (WaitLobbyTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(WaitLobbyTickerHandle);
		WaitLobbyTickerHandle.Reset();
	}
	// 검색 재시도 Ticker 해제 — 이것을 빠뜨리면 소멸 후 람다가 실행되어 댕글링 포인터 크래시 발생
	if (FindRetryTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(FindRetryTickerHandle);
		FindRetryTickerHandle.Reset();
	}
}

bool FExListenServerStrategy::CheckLobbyWaitConditions_Host(float DeltaTime)
{
	// 절대 시간 기반 경과 계산 — DeltaTime 누적 오차 없음
	const double Elapsed = FPlatformTime::Seconds() - WaitStartTime;

	IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
	if (!OSS || !OSS->GetSessionInterface().IsValid())
	{
		return true;
	}

	FNamedOnlineSession* Session = OSS->GetSessionInterface()->GetNamedSession(ExMatchSessionName);
	if (!Session)
	{
		UE_LOG(LogExNetwork, Warning, TEXT("[ExListenServerStrategy] Host 대기: Session 소멸 — 타임아웃 처리."));
		WaitLobbyTickerHandle.Reset();
		if (LobbyProvider) LobbyProvider->DestroyLobby();
		if (CachedOnComplete) { CachedOnComplete(false, TEXT("Timeout")); CachedOnComplete = nullptr; }
		return false;
	}

	const int32 MaxPlayers = Session->SessionSettings.NumPublicConnections;
	const int32 CurrentPlayers = MaxPlayers - Session->NumOpenPublicConnections;

	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Host 대기 중 — %d / %d 명, 경과 %.1f초"),
		CurrentPlayers, CurrentWaitConfig.ExpectedPlayerCount, Elapsed);

	if (CurrentWaitConfig.bIsSinglePlay || CurrentPlayers >= CurrentWaitConfig.ExpectedPlayerCount)
	{
		// 정원 충족 — MATCH_STARTED 세션 속성 설정 (클라이언트 감지용)
		FOnlineSessionSettings* Settings = OSS->GetSessionInterface()->GetSessionSettings(ExMatchSessionName);
		if (Settings)
		{
			Settings->Set(FName("MATCH_STARTED"), 1, EOnlineDataAdvertisementType::ViaOnlineService);
			OSS->GetSessionInterface()->UpdateSession(ExMatchSessionName, *Settings, true);
		}
		WaitLobbyTickerHandle.Reset();
		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Host 정원 충족 — 매칭 완료."));
		if (CachedOnComplete) { CachedOnComplete(true, TEXT("")); CachedOnComplete = nullptr; }
		return false;
	}

	if (Elapsed >= CurrentWaitConfig.MaxWaitForPlayersSeconds)
	{
		UE_LOG(LogExNetwork, Warning, TEXT("[ExListenServerStrategy] Host 대기 타임아웃 (%.1f초)."), Elapsed);
		WaitLobbyTickerHandle.Reset();
		if (LobbyProvider) LobbyProvider->DestroyLobby();
		if (CachedOnComplete) { CachedOnComplete(false, TEXT("Timeout")); CachedOnComplete = nullptr; }
		return false;
	}

	return true;
}

bool FExListenServerStrategy::CheckLobbyWaitConditions_Client(float DeltaTime)
{
	const double Elapsed = FPlatformTime::Seconds() - WaitStartTime;

	IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
	if (!OSS || !OSS->GetSessionInterface().IsValid())
	{
		return true;
	}

	FOnlineSessionSettings* Settings = OSS->GetSessionInterface()->GetSessionSettings(ExMatchSessionName);
	if (Settings)
	{
		int32 MatchStarted = 0;
		const bool bHasProperty = Settings->Get(FName("MATCH_STARTED"), MatchStarted);
		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Client 폴링 — MATCH_STARTED 존재=%d 값=%d 경과=%.1f초"),
			bHasProperty, MatchStarted, Elapsed);

		if (bHasProperty && MatchStarted == 1)
		{
			WaitLobbyTickerHandle.Reset();
			UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Client MATCH_STARTED 감지 — 매칭 완료."));
			if (CachedOnComplete) { CachedOnComplete(true, TEXT("")); CachedOnComplete = nullptr; }
			return false;
		}
	}
	else
	{
		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Client 폴링 — SessionSettings 없음, 경과=%.1f초"), Elapsed);
	}

	if (Elapsed >= CurrentWaitConfig.MaxWaitForPlayersSeconds)
	{
		WaitLobbyTickerHandle.Reset();
		if (LobbyProvider) LobbyProvider->DestroyLobby();
		if (CachedOnComplete) { CachedOnComplete(false, TEXT("Timeout")); CachedOnComplete = nullptr; }
		return false;
	}

	return true;
}
