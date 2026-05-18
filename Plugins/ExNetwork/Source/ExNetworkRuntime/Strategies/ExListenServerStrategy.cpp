// Copyright ExFrameWork. All Rights Reserved.

#include "ExListenServerStrategy.h"
#include "Core/ExNetworkLog.h"
#include "Providers/IExLobbyProvider.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
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
	bIsDestroyed = true; // UpdateSession 지연 콜백이 도달해도 즉시 반환하도록 플래그 설정

	// UpdateSession 핸들 해제 — ServerTravel 후 비동기 콜백이 소멸된 this를 역참조하는 크래시 방지
	if (UpdateSessionHandle.IsValid())
	{
		IOnlineSubsystem* OSS = OSSInstance;
		if (!OSS)
		{
			OSS = IOnlineSubsystem::Get(FName(TEXT("EOS")));
			if (!OSS)
			{
				OSS = IOnlineSubsystem::Get();
			}
		}
		if (OSS)
		{
			if (OSS->GetSessionInterface().IsValid())
			{
				OSS->GetSessionInterface()->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionHandle);
			}
		}
		UpdateSessionHandle.Reset();
	}

	ClearWaitLobbyTicker();

	// [댕글링 포인터 크래시 방지] Strategy 소멸 시 LobbyProvider의 모든 델리게이트를 확실하게 정리
	if (LobbyProvider)
	{
		LobbyProvider->OnFindComplete.Clear();
		LobbyProvider->OnCreateComplete.Clear();
		LobbyProvider->OnJoinComplete.Clear();
	}

	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] 소멸됨 — Ticker 및 LobbyProvider 델리게이트 해제 완료."));
}

EExServerType FExListenServerStrategy::GetServerType() const
{
	return EExServerType::ListenServer;
}

void FExListenServerStrategy::SetLobbyProvider(TSharedPtr<IExLobbyProvider> InLobbyProvider)
{
	LobbyProvider = InLobbyProvider;
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

	// ServerTravel 실행 먼저, 이후 5초 후에 로비 파괴
	// 리즘: ServerTravel 직후에 파괴하면 클라이언트가 P2P 핸드쉘이크 완료 전 세션 정보를 잃어버릴 수 있음.
	// 5초 후에는 코드가 월드에 주보 할당되어 있지 않을 수 있으므로, 럈다 폰소로 진행.
	if (LobbyProvider && LobbyProvider->IsInLobby())
	{
		TWeakPtr<IExLobbyProvider> WeakLobbyProvider = LobbyProvider;
		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([WeakLobbyProvider](float) -> bool
			{
				TSharedPtr<IExLobbyProvider> SharedLobbyProvider = WeakLobbyProvider.Pin();
				if (SharedLobbyProvider && SharedLobbyProvider->IsInLobby())
				{
					UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] ServerTravel 후 로비 정리 실행."));
					SharedLobbyProvider->DestroyLobby();
				}
				return false; // 일회성
			}),
			5.0f // 5초 후 실행 — 클라이언트 P2P 핸드쉘이크 완료 시간 보장
		);
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
	UE_LOG(LogExNetwork, Warning, TEXT("[ExListenServerStrategy] FindAndJoinOrCreate는 Deprecated 되었습니다. UExOnlineSubsystem의 FSM을 사용하세요."));
}

void FExListenServerStrategy::BeginSearchPhase(const FExMatchConfig& Config, EExMatchState ExpectedState, TFunction<void(bool, const FString&)> OnSearchComplete)
{
	CurrentWaitConfig = Config;
	if (!ensureMsgf(LobbyProvider, TEXT("[ExListenServerStrategy] BeginSearchPhase: LobbyProvider 없음.")))
	{
		OnSearchComplete(false, TEXT("LobbyProvider not set"));
		return;
	}

	if (Config.bIsSinglePlay)
	{
		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Single Play 모드 — 검색 스킵 (Timeout 처리)."));
		if (OnSearchComplete) OnSearchComplete(false, TEXT("Timeout"));
		return;
	}

	// 검색 첫 시도시점 캐싱 (재시도 중에는 시간 유지)
	if (FindRetryCount == 0)
	{
		WaitStartTime = FPlatformTime::Seconds();
		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Search Phase 시작 — Lobby 검색 중..."));
	}

	LobbyProvider->OnFindComplete.Clear();
	LobbyProvider->OnFindComplete.AddLambda(
		[this, ExpectedState, OnSearchComplete](bool bSuccess, int32 ResultCount)
		{
			// [경쟁 조건 해결 및 크래시 방지] 
			// 델리게이트 완료 이벤트를 다음 틱으로 지연하여 안전하게 브로드캐스트를 종료하고 
			// 현재 람다의 실행 및 델리게이트 인스턴스 할당 해제(UAF) 크래시를 완벽히 방지합니다.
			if (bIsDestroyed) return;

			SearchPhaseTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateLambda([this, ExpectedState, OnSearchComplete, bSuccess, ResultCount](float) -> bool
				{
					SearchPhaseTickerHandle.Reset();
					if (bIsDestroyed) return false;

					if (bSuccess && ResultCount > 0)
					{
						UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Lobby %d개 발견."), ResultCount);
						FindRetryCount = 0; // 다음을 위해 초기화
						if (OnSearchComplete) OnSearchComplete(true, TEXT(""));
					}
					else
					{
						const double Elapsed = FPlatformTime::Seconds() - WaitStartTime;
						if (Elapsed >= CurrentWaitConfig.MaxWaitForPlayersSeconds)
						{
							UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] %.1f초 대기 후 Lobby 없음 — Search 타임아웃."), Elapsed);
							FindRetryCount = 0;
							if (OnSearchComplete) OnSearchComplete(false, TEXT("Timeout"));
						}
						else
						{
							FindRetryCount++;
							UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Lobby 없음 — %.1f초 후 재검색 시도 (%d회). 경과=%.1f/%.1f초"),
								FindRetryDelay, FindRetryCount, Elapsed, CurrentWaitConfig.MaxWaitForPlayersSeconds);

							FindRetryTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
								FTickerDelegate::CreateLambda([this, ExpectedState, OnSearchComplete](float) -> bool
								{
									FindRetryTickerHandle.Reset();
									if (bIsDestroyed) return false;
									if (!OnSearchComplete) return false;
									this->BeginSearchPhase(CurrentWaitConfig, ExpectedState, OnSearchComplete);
									return false;
								}),
								FindRetryDelay
							);
						}
					}
					return false; // 일회성
				}),
				0.0f // 즉시 다음 틱에 실행
			);
		});
	LobbyProvider->FindLobbies(CurrentWaitConfig);
}

void FExListenServerStrategy::EndSearchPhase()
{
	if (FindRetryTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(FindRetryTickerHandle);
		FindRetryTickerHandle.Reset();
	}
	if (LobbyProvider)
	{
		LobbyProvider->OnFindComplete.Clear();
	}
}

void FExListenServerStrategy::BeginCreatePhase(const FExMatchConfig& Config, EExMatchState ExpectedState, TFunction<void(bool, const FString&)> OnCreateComplete)
{
	CurrentWaitConfig = Config;
	bIsHost = true;

	if (!ensureMsgf(LobbyProvider, TEXT("[ExListenServerStrategy] BeginCreatePhase: LobbyProvider 없음.")))
	{
		OnCreateComplete(false, TEXT("LobbyProvider not set"));
		return;
	}

	LobbyProvider->OnCreateComplete.Clear();
	LobbyProvider->OnCreateComplete.AddLambda(
		[this, OnCreateComplete](bool bCreateSuccess, const FString& ErrorMessage)
		{
			// [경쟁 조건 해결 및 크래시 방지]
			// 델리게이트 완료 이벤트를 다음 틱으로 지연하여 안전하게 브로드캐스트를 종료하고
			// 현재 람다의 실행 및 델리게이트 인스턴스 할당 해제(UAF) 크래시를 완벽히 방지합니다.
			CreatePhaseTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateLambda([this, OnCreateComplete, bCreateSuccess, ErrorMessage](float) -> bool
				{
					CreatePhaseTickerHandle.Reset();
					if (bIsDestroyed) return false;

					if (bCreateSuccess)
					{
						UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Lobby 생성 성공."));
						OnCreateComplete(true, TEXT(""));
					}
					else
					{
						UE_LOG(LogExNetwork, Warning, TEXT("[ExListenServerStrategy] Lobby 생성 실패 — %s"), *ErrorMessage);
						OnCreateComplete(false, ErrorMessage);
					}
					return false; // 일회성
				}),
				0.0f
			);
		}
	);
	LobbyProvider->CreateLobby(CurrentWaitConfig);
}

void FExListenServerStrategy::EndCreatePhase()
{
	if (LobbyProvider)
	{
		LobbyProvider->OnCreateComplete.Clear();
	}
}

void FExListenServerStrategy::BeginJoinPhase(const FExMatchConfig& Config, const FString& SessionId, EExMatchState ExpectedState, TFunction<void(bool, const FString&)> OnJoinComplete)
{
	CurrentWaitConfig = Config;
	bIsHost = false;

	if (!ensureMsgf(LobbyProvider, TEXT("[ExListenServerStrategy] BeginJoinPhase: LobbyProvider 없음.")))
	{
		OnJoinComplete(false, TEXT("LobbyProvider not set"));
		return;
	}

	LobbyProvider->OnJoinComplete.Clear();
	LobbyProvider->OnJoinComplete.AddLambda(
		[this, OnJoinComplete](bool bJoinSuccess, const FString& ErrorMessage)
		{
			// [경쟁 조건 해결 및 크래시 방지]
			// 델리게이트 완료 이벤트를 다음 틱으로 지연하여 안전하게 브로드캐스트를 종료하고
			// 현재 람다의 실행 및 델리게이트 인스턴스 할당 해제(UAF) 크래시를 완벽히 방지합니다.
			JoinPhaseTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateLambda([this, OnJoinComplete, bJoinSuccess, ErrorMessage](float) -> bool
				{
					JoinPhaseTickerHandle.Reset();
					if (bIsDestroyed) return false;

					if (bJoinSuccess)
					{
						CachedConnectString = LobbyProvider->GetConnectString();
						UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Lobby 참가 성공 — ConnectString 캐시: %s"), *CachedConnectString);
						OnJoinComplete(true, TEXT(""));
					}
					else
					{
						UE_LOG(LogExNetwork, Warning, TEXT("[ExListenServerStrategy] Lobby 참가 실패 — %s"), *ErrorMessage);
						OnJoinComplete(false, ErrorMessage);
					}
					return false; // 일회성
				}),
				0.0f
			);
		}
	);
	// SessionId는 현재 0번 인덱스를 의미함 (Lobby 0번 참가)
	LobbyProvider->JoinLobby(0);
}

void FExListenServerStrategy::EndJoinPhase()
{
	if (LobbyProvider)
	{
		LobbyProvider->OnJoinComplete.Clear();
	}
}

void FExListenServerStrategy::BeginWaitPhase(const FExMatchConfig& Config, bool bIsHostFlag, EExMatchState ExpectedState, TFunction<void(bool, const FString&)> OnReadyCallback)
{
	CurrentWaitConfig = Config;
	WaitStartTime = FPlatformTime::Seconds(); // Wait phase부터 새로 카운트
	CachedOnComplete = OnReadyCallback;

	if (bIsHostFlag)
	{
		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Host 대기 루프 시작."));
		WaitLobbyTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateRaw(this, &FExListenServerStrategy::CheckLobbyWaitConditions_Host), 1.0f);
	}
	else
	{
		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Client 대기 루프 시작."));
		WaitLobbyTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateRaw(this, &FExListenServerStrategy::CheckLobbyWaitConditions_Client), 1.0f);
	}
}

void FExListenServerStrategy::EndWaitPhase()
{
	if (WaitLobbyTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(WaitLobbyTickerHandle);
		WaitLobbyTickerHandle.Reset();
	}
	CachedOnComplete = nullptr;
}

void FExListenServerStrategy::ResetTransientState()
{
	FindRetryCount = 0;
	ClearWaitLobbyTicker();
	
	if (LobbyProvider)
	{
		LobbyProvider->OnFindComplete.Clear();
		LobbyProvider->OnCreateComplete.Clear();
		LobbyProvider->OnJoinComplete.Clear();
		
		const bool bHasSession = LobbyProvider->HasLocalSession();
		const bool bInLobby = LobbyProvider->IsInLobby();
		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] ResetTransientState — HasLocalSession=%d, IsInLobby=%d"),
			bHasSession, bInLobby);
		
		if (bHasSession)
		{
			UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] ResetTransientState: 로컈 세션 파괴."));
			LobbyProvider->DestroyLobby();
		}
	}
	else
	{
		UE_LOG(LogExNetwork, Warning, TEXT("[ExListenServerStrategy] ResetTransientState: LobbyProvider 없음."));
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

		const bool bInLobby = LobbyProvider->IsInLobby();
		const bool bHasSession = LobbyProvider->HasLocalSession();
		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] CancelMatch — IsInLobby=%d, HasLocalSession=%d"),
			bInLobby, bHasSession);

		if (bInLobby)
		{
			LobbyProvider->DestroyLobby();
		}
	}
	FindRetryCount = 0;
	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] 매칭 취소 완료."));
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
	// 비동기 Phase 지연 완료 Ticker 해제
	if (SearchPhaseTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(SearchPhaseTickerHandle);
		SearchPhaseTickerHandle.Reset();
	}
	if (CreatePhaseTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(CreatePhaseTickerHandle);
		CreatePhaseTickerHandle.Reset();
	}
	if (JoinPhaseTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(JoinPhaseTickerHandle);
		JoinPhaseTickerHandle.Reset();
	}
}

bool FExListenServerStrategy::CheckLobbyWaitConditions_Host(float DeltaTime)
{
	// 절대 시간 기반 경과 계산 — DeltaTime 누적 오차 없음
	const double Elapsed = FPlatformTime::Seconds() - WaitStartTime;

	IOnlineSubsystem* OSS = OSSInstance;
	if (!OSS)
	{
		OSS = IOnlineSubsystem::Get(FName(TEXT("EOS")));
		if (!OSS)
		{
			OSS = IOnlineSubsystem::Get();
		}
	}
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
		// 정원 충족 — MATCH_STARTED 세션 속성 설정 후 UpdateSession 완료를 기다린 뒤 OnComplete 호출
		FOnlineSessionSettings* Settings = OSS->GetSessionInterface()->GetSessionSettings(ExMatchSessionName);
		if (!Settings && Session)
		{
			Settings = &Session->SessionSettings;
		}
		if (Settings)
		{
			// int32(1) 대신 bool(true) 사용 — EOS SDK에서 int32가 int64로 강제 변환되어 클라이언트에서 읽지 못하는 현상 방지
			Settings->Set(FName("MATCH_STARTED"), true, EOnlineDataAdvertisementType::ViaOnlineService);
			
			// 매칭이 완료되었으므로 더 이상 검색에 노출되지 않도록 설정 (로비 파괴 대신 사용)
			Settings->bShouldAdvertise = false;
			Settings->bAllowJoinInProgress = false;

			// UpdateSessionHandle을 멤버로 관리 — 소멸자에서 해제하여 ServerTravel 후 댕글링 크래시 방지
			UpdateSessionHandle = OSS->GetSessionInterface()->AddOnUpdateSessionCompleteDelegate_Handle(
				FOnUpdateSessionCompleteDelegate::CreateLambda(
					[this, OSS](FName SessionName, bool bUpdateSuccess)
					{
						// [경쟁 조건 및 UAF 크래시 방지]
						// 델리게이트 완료 핸들 해제 및 후속 처리를 다음 틱으로 지연하여 
						// 현재 실행 중인 람다 클로저가 브로드캐스트 스택 내부에서 동기적으로 소멸(Use-After-Free)되는 것을 원천 방지합니다.
						FTSTicker::GetCoreTicker().AddTicker(
							FTickerDelegate::CreateLambda([this, OSS, SessionName, bUpdateSuccess](float) -> bool
							{
								if (bIsDestroyed) return false;

								OSS->GetSessionInterface()->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionHandle);
								UpdateSessionHandle.Reset();

								UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Host MATCH_STARTED 업데이트 완료(bSuccess=%d) — 매칭 완료 콜백 호출."), bUpdateSuccess);
								
								if (CachedOnComplete) 
								{ 
									// EOS SDK 콜백 스택(TriggerOnUpdateSessionCompleteDelegates) 내부에서
									// DestroySession이나 ServerTravel이 동기적으로 실행되는 것을 방지하기 위해 지연 호출 실행
									TFunction<void(bool, const FString&)> TempComplete = MoveTemp(CachedOnComplete);
									CachedOnComplete = nullptr;
									
									FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([TempComplete](float) -> bool
									{
										if (TempComplete)
										{
											TempComplete(true, TEXT(""));
										}
										return false;
									}), 0.5f);
								}
								return false; // 일회성
							}),
							0.0f
						);
					}
				)
			);
			OSS->GetSessionInterface()->UpdateSession(ExMatchSessionName, *Settings, true);
		}
		else
		{
			// Settings를 가져올 수 없는 경우 즉시 완료 처리
			UE_LOG(LogExNetwork, Warning, TEXT("[ExListenServerStrategy] SessionSettings 없음 — 즉시 완료 처리."));
			if (CachedOnComplete) { CachedOnComplete(true, TEXT("")); CachedOnComplete = nullptr; }
		}
		WaitLobbyTickerHandle.Reset();
		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Host 정원 충족 — 매칭 완료."));
		return false;
	}

	if (Elapsed >= CurrentWaitConfig.MaxWaitForPlayersSeconds)
	{
		UE_LOG(LogExNetwork, Warning, TEXT("[ExListenServerStrategy] Host 대기 타임아웃 (%.1f초)."), Elapsed);
		WaitLobbyTickerHandle.Reset();
		// DestroyLobby는 여기서 직접 호출하지 않음 — 이중 파괴 방지.
		// HasLocalSession 경로에서 다음 FindAndJoinOrCreate 시 처리.
		if (CachedOnComplete) { CachedOnComplete(false, TEXT("Timeout")); CachedOnComplete = nullptr; }
		return false;
	}

	return true;
}

bool FExListenServerStrategy::CheckLobbyWaitConditions_Client(float DeltaTime)
{
	const double Elapsed = FPlatformTime::Seconds() - WaitStartTime;

	IOnlineSubsystem* OSS = OSSInstance;
	if (!OSS)
	{
		OSS = IOnlineSubsystem::Get(FName(TEXT("EOS")));
		if (!OSS)
		{
			OSS = IOnlineSubsystem::Get();
		}
	}
	if (!OSS || !OSS->GetSessionInterface().IsValid())
	{
		return true;
	}

	// [진단 로그] 주인님을 위한 고유 세션 진단 정보 출력
	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] [진단] SubsystemName=%s"), *OSS->GetSubsystemName().ToString());
	
	FNamedOnlineSession* NamedSession = OSS->GetSessionInterface()->GetNamedSession(ExMatchSessionName);
	if (NamedSession)
	{
		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] [진단] NamedSession 'ExMatch' 발견! State=%s, SessionId=%s"),
			EOnlineSessionState::ToString(NamedSession->SessionState),
			NamedSession->SessionInfo.IsValid() ? *NamedSession->SessionInfo->GetSessionId().ToString() : TEXT("INVALID"));
		
		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] [진단] SessionSettings의 설정 개수: %d"), NamedSession->SessionSettings.Settings.Num());
		for (const auto& KVP : NamedSession->SessionSettings.Settings)
		{
			UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] [진단]   Key=%s, AdvType=%d, ValueType=%d, Value=%s"),
				*KVP.Key.ToString(), (int)KVP.Value.AdvertisementType, (int)KVP.Value.Data.GetType(), *KVP.Value.Data.ToString());
		}
	}
	else
	{
		UE_LOG(LogExNetwork, Warning, TEXT("[ExListenServerStrategy] [진단] NamedSession 'ExMatch' 없음!"));
	}

	FOnlineSessionSettings* Settings = OSS->GetSessionInterface()->GetSessionSettings(ExMatchSessionName);
	if (!Settings && NamedSession)
	{
		Settings = &NamedSession->SessionSettings;
	}
	if (Settings)
	{
		bool bMatchStarted = false;
		bool bHasProperty = false;
		const FOnlineSessionSetting* SettingInfo = Settings->Settings.Find(FName("MATCH_STARTED"));
		if (SettingInfo)
		{
			bHasProperty = true;
			const FVariantData& SettingData = SettingInfo->Data;
			if (SettingData.GetType() == EOnlineKeyValuePairDataType::Bool)
			{
				SettingData.GetValue(bMatchStarted);
			}
			else if (SettingData.GetType() == EOnlineKeyValuePairDataType::Int32)
			{
				int32 Val = 0;
				SettingData.GetValue(Val);
				bMatchStarted = (Val == 1);
			}
			else if (SettingData.GetType() == EOnlineKeyValuePairDataType::Int64)
			{
				int64 Val = 0;
				SettingData.GetValue(Val);
				bMatchStarted = (Val == 1);
			}
		}

		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Client 폴링 — MATCH_STARTED 존재=%d 값=%d 경과=%.1f초"),
			bHasProperty, bMatchStarted, Elapsed);

		if (bHasProperty && bMatchStarted)
		{
			WaitLobbyTickerHandle.Reset();
			UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Client MATCH_STARTED 감지 — UExOnlineSubsystem으로 제어권 위임 (ConnectString=%s)"), *CachedConnectString);
			
			if (CachedOnComplete) 
			{ 
				CachedOnComplete(true, CachedConnectString); 
				CachedOnComplete = nullptr; 
			}
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
		// DestroyLobby는 여기서 직접 호출하지 않음.
		// CachedOnComplete → FindQuickMatch → FindAndJoinOrCreate → HasLocalSession 경로에서 처리.
		// 여기서 호출하면 이중 파괴(double destroy)가 발생하여 MatchMode 문자열 오염의 원인이 됨.
		if (CachedOnComplete) { CachedOnComplete(false, TEXT("Timeout")); CachedOnComplete = nullptr; }
		return false;
	}

	return true;
}
