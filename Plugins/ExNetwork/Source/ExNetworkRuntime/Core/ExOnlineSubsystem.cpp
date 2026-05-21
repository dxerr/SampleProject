// Copyright ExFrameWork. All Rights Reserved.

#include "ExOnlineSubsystem.h"
#include "ExNetworkLog.h"
#include "IExAuthProvider.h"
#include "IExNetServerStrategy.h"
#include "Providers/EOS/ExEOSAuthProvider.h"
#include "Providers/Null/ExNullAuthProvider.h"
#include "Providers/EOS/ExEOSLobbyProvider.h"
#include "Strategies/ExListenServerStrategy.h"
#include "Strategies/ExDedicatedServerStrategy.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineDelegates.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"

void UExOnlineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] Initialize 시작."));

	// [중요] ClientTravel 후 World가 교체될 때 GameInstanceSubsystem이 재초기화되는 경우가 있음.
	// 이미 InGame 상태라면 EOS 세션과 Strategy가 살아있으므로 재초기화를 건너뜀.
	if (CurrentMatchState == EExMatchState::InGame && AuthProvider && ServerStrategy)
	{
		UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] InGame 상태에서 Initialize 재호출 — 재초기화 건너뜀 (세션 보존)."));
		return;
	}

	BuildTransitionMap();

	const UWorld* World = GetWorld();
	const bool bIsDedicatedServer = (World && World->GetNetMode() == NM_DedicatedServer);

	if (bIsDedicatedServer)
	{
		ServerStrategy = MakeUnique<FExDedicatedServerStrategy>();
		UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] ServerStrategy 선택: DedicatedServer"));
	}
	else
	{
		ServerStrategy = MakeUnique<FExListenServerStrategy>();
		UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] ServerStrategy 선택: ListenServer"));
	}

	SubsystemCreatedHandle = FOnlineSubsystemDelegates::OnOnlineSubsystemCreated.AddUObject(
		this, &UExOnlineSubsystem::HandleOnlineSubsystemCreated
	);

	IOnlineSubsystem* OSS = TryGetEOSSubsystem();
	if (OSS && OSS->GetSubsystemName() != FName("NULL") && !AuthProvider)
	{
		UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] EOS OSS 즉시 획득 — 직접 초기화 진행."));
		FOnlineSubsystemDelegates::OnOnlineSubsystemCreated.Remove(SubsystemCreatedHandle);
		SubsystemCreatedHandle.Reset();
		InitAuthProviderAndLogin(OSS);
	}

	NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UExOnlineSubsystem::HandleNetworkFailure);
}

IOnlineSubsystem* UExOnlineSubsystem::TryGetEOSSubsystem() const
{
	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld(), FName(TEXT("EOS")));
	if (!OSS)
	{
		OSS = IOnlineSubsystem::Get(TEXT("EOS"));
	}
	if (OSS && OSS->GetSubsystemName() != FName("NULL"))
	{
		return OSS;
	}
	return nullptr;
}

void UExOnlineSubsystem::HandleOnlineSubsystemCreated(IOnlineSubsystem* NewSubsystem)
{
	if (!NewSubsystem || NewSubsystem->GetSubsystemName() != FName("EOS"))
	{
		return;
	}

	if (IOnlineSubsystem* WorldOSS = Online::GetSubsystem(GetWorld(), FName(TEXT("EOS"))))
	{
		if (WorldOSS != NewSubsystem)
		{
			UE_LOG(LogExNetwork, Verbose, TEXT("[UExOnlineSubsystem] 생성된 EOS OSS(%p)가 현재 월드 OSS(%p)와 달라 무시합니다."), NewSubsystem, WorldOSS);
			return;
		}
	}

	UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] OnOnlineSubsystemCreated 콜백 — EOS OSS 생성 확인."));
	FOnlineSubsystemDelegates::OnOnlineSubsystemCreated.Remove(SubsystemCreatedHandle);
	SubsystemCreatedHandle.Reset();
	InitAuthProviderAndLogin(NewSubsystem);
}

void UExOnlineSubsystem::InitAuthProviderAndLogin(IOnlineSubsystem* OSS)
{
	check(OSS);
	UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] OSS 확정 — 서브시스템명: %s"), *OSS->GetSubsystemName().ToString());

	AuthProvider = MakeUnique<FExEOSAuthProvider>(OSS);
	UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] AuthProvider 선택: EOS"));

	if (FExListenServerStrategy* ListenStrategy = static_cast<FExListenServerStrategy*>(ServerStrategy.Get()))
	{
		if (ListenStrategy->GetServerType() == EExServerType::ListenServer)
		{
			ListenStrategy->SetOnlineSubsystem(OSS);
			ListenStrategy->SetLobbyProvider(MakeShared<FExEOSLobbyProvider>(OSS));
			UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] LobbyProvider 주입 완료: ExEOSLobbyProvider"));
		}
	}

	AuthProvider->OnLoginComplete.AddUObject(this, &UExOnlineSubsystem::HandleAuthLoginComplete);

	UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] 자동 로그인 시작..."));
	AuthProvider->Login(0);
}

void UExOnlineSubsystem::Deinitialize()
{
	UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] Deinitialize."));

	// [중요] InGame 상태에서 Deinitialize가 호출되면 (ClientTravel/ServerTravel 월드 교체)
	// EOS 세션과 Strategy를 파괴하지 않고 보존해야 P2P 연결이 유지됩니다.
	// GameInstance 수명이 끝나는 진짜 종료 시에는 InGame 상태가 아니므로 정상 정리됩니다.
	// [예외] PIE 종료 시: PIE 컨텍스트에서는 세션 보존이 불필요하므로 완전 정리합니다.
	const bool bIsPIESession = GetGameInstance() && GetGameInstance()->GetWorld()
		&& GetGameInstance()->GetWorld()->IsPlayInEditor();
	if (CurrentMatchState == EExMatchState::InGame && !bIsPIESession)
	{
		UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] InGame 상태에서 Deinitialize — Strategy/Session 보존 (P2P 연결 유지)."));
		// 델리게이트 핸들만 정리하고 세션과 Strategy는 유지
		if (SubsystemCreatedHandle.IsValid())
		{
			FOnlineSubsystemDelegates::OnOnlineSubsystemCreated.Remove(SubsystemCreatedHandle);
			SubsystemCreatedHandle.Reset();
		}
		if (NetworkFailureHandle.IsValid())
		{
			GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
			NetworkFailureHandle.Reset();
		}
		if (PendingMatchStateTickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(PendingMatchStateTickerHandle);
			PendingMatchStateTickerHandle.Reset();
		}
		Super::Deinitialize();
		return;
	}

	if (SubsystemCreatedHandle.IsValid())
	{
		FOnlineSubsystemDelegates::OnOnlineSubsystemCreated.Remove(SubsystemCreatedHandle);
		SubsystemCreatedHandle.Reset();
	}

	if (NetworkFailureHandle.IsValid())
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		NetworkFailureHandle.Reset();
	}

	if (AuthProvider)
	{
		AuthProvider->OnLoginComplete.RemoveAll(this);
	}

	AuthProvider.Reset();
	ServerStrategy.Reset();

	TransitionMatchState(EExMatchState::Idle, ETransitionReason::Deinitialize);
	if (PendingMatchStateTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PendingMatchStateTickerHandle);
		PendingMatchStateTickerHandle.Reset();
	}

	Super::Deinitialize();
}

bool UExOnlineSubsystem::IsLoggedIn() const
{
	return AuthProvider ? AuthProvider->IsLoggedIn(0) : false;
}

FString UExOnlineSubsystem::GetServerTypeString() const
{
	if (!ServerStrategy) return TEXT("None");
	return (ServerStrategy->GetServerType() == EExServerType::ListenServer)
		? TEXT("ListenServer") : TEXT("DedicatedServer");
}

bool UExOnlineSubsystem::IsMatchInProgress() const
{
	return CurrentMatchState != EExMatchState::Idle && CurrentMatchState != EExMatchState::InGame;
}

bool UExOnlineSubsystem::IsMatchReadyToStart() const
{
	return CurrentMatchState == EExMatchState::Ready;
}

bool UExOnlineSubsystem::IsMatchActive() const
{
	return CurrentMatchState == EExMatchState::InGame;
}

void UExOnlineSubsystem::DebugForceMatchState(EExMatchState NewState)
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogExNetwork, Warning, TEXT("[UExOnlineSubsystem] DebugForceMatchState 호출됨: %d"), (int32)NewState);
	TransitionMatchState(NewState, ETransitionReason::UserRequest);
#endif
}

void UExOnlineSubsystem::FindQuickMatch(const FExMatchConfig& Config)
{
	if (!IsLoggedIn())
	{
		UE_LOG(LogExNetwork, Warning, TEXT("[UExOnlineSubsystem] FindQuickMatch: 로그인 미완료."));
		OnMatchFound.Broadcast(false, TEXT("Not logged in"));
		return;
	}

	if (CurrentMatchState != EExMatchState::Idle)
	{
		if (CurrentMatchState == EExMatchState::InGame)
		{
			UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] FindQuickMatch: 이전 게임 상태가 남아있어 Idle로 초기화 후 진행합니다."));
			ResetMatchState();
		}
		else
		{
			UE_LOG(LogExNetwork, Warning, TEXT("[UExOnlineSubsystem] FindQuickMatch: 이미 매칭 진행 중. State=%d"), (int32)CurrentMatchState);
			OnMatchFound.Broadcast(false, TEXT("Already matching"));
			return;
		}
	}

	FExListenServerStrategy* ListenStrategy = static_cast<FExListenServerStrategy*>(ServerStrategy.Get());
	if (!ensureMsgf(ListenStrategy, TEXT("[UExOnlineSubsystem] FindQuickMatch: ListenServerStrategy 없음.")))
	{
		OnMatchFound.Broadcast(false, TEXT("Strategy not available"));
		return;
	}

	PendingMatchConfig = Config;

	if (Config.bIsSinglePlay)
	{
		UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] FindQuickMatch: SinglePlay 모드 — 즉시 Creating 진입."));
		TransitionMatchState(EExMatchState::Creating, ETransitionReason::UserRequest);
	}
	else
	{
		UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] FindQuickMatch 시작 — MatchMode=%s"), *Config.MatchMode);
		TransitionMatchState(EExMatchState::Searching, ETransitionReason::UserRequest);
	}
}

void UExOnlineSubsystem::CancelMatch()
{
	if (CurrentMatchState == EExMatchState::InGame)
	{
		UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] CancelMatch: 이미 게임(InGame) 상태이므로 매칭 취소를 무시합니다."));
		return;
	}

	FExListenServerStrategy* ListenStrategy = static_cast<FExListenServerStrategy*>(ServerStrategy.Get());
	if (ListenStrategy)
	{
		ListenStrategy->CancelMatch();
	}
	TransitionMatchState(EExMatchState::Idle, ETransitionReason::Cancel);
	UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] CancelMatch 완료."));
}

void UExOnlineSubsystem::ResetMatchState()
{
	UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] ResetMatchState: 매칭 상태를 강제로 초기화합니다."));
	FExListenServerStrategy* ListenStrategy = static_cast<FExListenServerStrategy*>(ServerStrategy.Get());
	if (ListenStrategy)
	{
		ListenStrategy->CancelMatch();
	}
	TransitionMatchState(EExMatchState::Idle, ETransitionReason::Reset);
}

void UExOnlineSubsystem::StartGame(const FExMatchConfig& Config)
{
	// 실패 조건 검증
	if (!IsLoggedIn())
	{
		UE_LOG(LogExNetwork, Warning, TEXT("[UExOnlineSubsystem] StartGame: 로그인 미완료."));
		OnGameStarted.Broadcast(false, TEXT("Not logged in"));
		return;
	}

	if (CurrentMatchState != EExMatchState::Ready)
	{
		UE_LOG(LogExNetwork, Warning, TEXT("[UExOnlineSubsystem] StartGame: 매칭 Ready 상태가 아님. State=%d"), (int32)CurrentMatchState);
		OnGameStarted.Broadcast(false, TEXT("Match not ready"));
		return;
	}

	if (Config.MapPath.IsEmpty())
	{
		UE_LOG(LogExNetwork, Warning, TEXT("[UExOnlineSubsystem] StartGame: MapPath가 비어있습니다."));
		OnGameStarted.Broadcast(false, TEXT("MapPath is empty"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogExNetwork, Warning, TEXT("[UExOnlineSubsystem] StartGame: World 없음."));
		OnGameStarted.Broadcast(false, TEXT("World not available"));
		return;
	}

	FExListenServerStrategy* ListenStrategy = static_cast<FExListenServerStrategy*>(ServerStrategy.Get());
	if (!ensureMsgf(ListenStrategy, TEXT("[UExOnlineSubsystem] StartGame: ListenServerStrategy 없음.")))
	{
		OnGameStarted.Broadcast(false, TEXT("ServerStrategy not available"));
		return;
	}

	TransitionMatchState(EExMatchState::InGame, ETransitionReason::UserRequest);
	UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] StartGame — MapPath=%s, IsHost=%d, NetMode=%d"),
		*Config.MapPath, ListenStrategy->IsHost(), (int32)World->GetNetMode());
	OnGameStarted.Broadcast(true, TEXT(""));

	if (ListenStrategy->IsHost())
	{
		// 호스트: 방 생성자이므로 ServerTravel을 통해 게임 맵으로 이동
		// URL 옵션 구성은 ServerStrategy 내부에서 순서대로 정교하게 구성하도록 맵 이름만 전달합니다.
		UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] StartGame [HOST] — ServerTravel 시작. Strategy로 경로 위임: %s"), *Config.MapPath);
		ListenStrategy->StartGameSession(Config.MapPath, World);
	}
	else
	{
		// 클라이언트: 로비에 참가한 입장이므로 호스트 세션으로 ClientTravel 수행
		FString ConnectString = ListenStrategy->GetConnectString();
		if (ConnectString.IsEmpty())
		{
			UE_LOG(LogExNetwork, Error, TEXT("[UExOnlineSubsystem] StartGame [CLIENT] — ConnectString이 비어있습니다! 로비 참가 실패 가능성이 있습니다."));
			return;
		}
		
		// [경쟁 조건 해결] 호스트가 ServerTravel을 완료하고 맵을 로드했는지 확인하기 위한 세션 프로퍼티 폴링 방식은 
		// 클라이언트의 로컬 세션 캐시(NamedSession)에 업데이트가 즉각 반영되지 않아 무한 대기(타임아웃)에 빠지는 문제가 발생합니다.
		// 따라서 클라이언트가 호스트 맵 로드(ServerTravel)가 완전히 끝나고 P2P Listen 소켓이 열릴 때까지 충분한 시간을 대기하도록 
		// 지연 시간을 기존 6초에서 20초(호스트의 에셋 프리로드 시간 감안)로 연장하여 접속을 시도합니다.
		UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] StartGame [CLIENT] — 호스트의 맵 로드 및 NetDriver 가동 대기를 위해 20.0초 후 ClientTravel을 지연 실행합니다."));
		
		TWeakObjectPtr<UExOnlineSubsystem> WeakSelf = this;
		
		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([WeakSelf, ConnectString](float) -> bool
			{
				if (UExOnlineSubsystem* Self = WeakSelf.Get())
				{
					UWorld* CurrentWorld = Self->GetWorld();
					if (CurrentWorld)
					{
						APlayerController* PC = CurrentWorld->GetFirstPlayerController();
						if (PC)
						{
							UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] StartGame [CLIENT] — 지연 대기 완료. ClientTravel 실행. URL=%s, PC=%s"),
								*ConnectString, *PC->GetName());
							PC->ClientTravel(ConnectString, TRAVEL_Absolute);
						}
						else
						{
							UE_LOG(LogExNetwork, Error, TEXT("[UExOnlineSubsystem] StartGame [CLIENT] — PlayerController를 찾을 수 없어 지연 ClientTravel 실패."));
						}
					}
				}
				return false; // 일회성
			}),
			20.0f // 20초 대기 (호스트 에셋 로딩 대기)
		);
	}
}


void UExOnlineSubsystem::HandleAuthLoginComplete(bool bSuccess, const FString& ErrorMessage)
{
	if (bSuccess)
	{
		UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] 로그인 완료 — 성공."));
	}
	else
	{
		UE_LOG(LogExNetwork, Warning, TEXT("[UExOnlineSubsystem] 로그인 완료 — 실패. Error=%s"), *ErrorMessage);
	}
	OnLoginComplete.Broadcast(bSuccess, ErrorMessage);
}

void UExOnlineSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(LogExNetwork, Error, TEXT("[UExOnlineSubsystem] NetworkFailure Detected: Type=%d, Message=%s"), (int32)FailureType, *ErrorString);

	// 매칭이나 게임 연결 상태에서 문제가 발생한 경우 리셋
	if (CurrentMatchState != EExMatchState::Idle)
	{
		UE_LOG(LogExNetwork, Warning, TEXT("[UExOnlineSubsystem] 클라이언트 접속 타임아웃/끊김. 매치 강제 리셋."));
		ResetMatchState();

		// UI 서브시스템 직접 참조 대신 델리게이트로 실패를 외부에 알립니다.
		OnMatchConnectionFailed.Broadcast(TEXT("네트워크 접속이 원활하지 않아 게임 서버 연결에 실패했습니다."));
	}
}

// ------------------------------------------------------------------
// FSM 로직
// ------------------------------------------------------------------

void UExOnlineSubsystem::BuildTransitionMap()
{
	TransitionMap.Empty();

	// Phase A 골격이므로, 허용 가능한 전이 목록만 정의 (엄격한 검사 가능)
	TransitionMap.Add(EExMatchState::Idle, { EExMatchState::Searching, EExMatchState::Creating });
	TransitionMap.Add(EExMatchState::Searching, { EExMatchState::Idle, EExMatchState::Creating, EExMatchState::Joining });
	TransitionMap.Add(EExMatchState::Creating, { EExMatchState::Idle, EExMatchState::Waiting });
	TransitionMap.Add(EExMatchState::Waiting, { EExMatchState::Idle, EExMatchState::Ready });
	TransitionMap.Add(EExMatchState::Joining, { EExMatchState::Idle, EExMatchState::Ready, EExMatchState::Waiting });
	TransitionMap.Add(EExMatchState::Ready, { EExMatchState::Idle, EExMatchState::InGame });
	TransitionMap.Add(EExMatchState::InGame, { EExMatchState::Idle });
}

bool UExOnlineSubsystem::IsTransitionAllowed(EExMatchState FromState, EExMatchState ToState) const
{
	if (FromState == ToState) return true; // Self-transition 허용 (동일 상태는 무시됨)
	if (ToState == EExMatchState::Idle) return true; // Idle로의 복귀는 항상 허용

	const TArray<EExMatchState>* AllowedTransitions = TransitionMap.Find(FromState);
	return AllowedTransitions && AllowedTransitions->Contains(ToState);
}

bool UExOnlineSubsystem::ShouldHonorCallback(EExMatchState ExpectedState) const
{
	if (CurrentMatchState != ExpectedState)
	{
		UE_LOG(LogExNetwork, Warning, TEXT("[UExMatchFSM] Stale callback ignored. Expected: %d, Current: %d"), (int32)ExpectedState, (int32)CurrentMatchState);
		return false;
	}
	return true;
}

void UExOnlineSubsystem::TransitionMatchState(EExMatchState NewState, ETransitionReason Reason)
{
	// 우선순위 판별 (Safety-First vs Normal)
	bool bIsNewReasonSafetyFirst = (Reason == ETransitionReason::Cancel || Reason == ETransitionReason::Reset || Reason == ETransitionReason::Deinitialize);
	
	if (bIsTransitioningMatchState)
	{
		// 재진입 감지됨
		if (PendingMatchStateTransition.IsSet())
		{
			bool bIsPendingSafetyFirst = (PendingMatchStateTransition.GetValue().Value == ETransitionReason::Cancel || 
										  PendingMatchStateTransition.GetValue().Value == ETransitionReason::Reset || 
										  PendingMatchStateTransition.GetValue().Value == ETransitionReason::Deinitialize);
			
			if (bIsPendingSafetyFirst && !bIsNewReasonSafetyFirst)
			{
				UE_LOG(LogExNetwork, Warning, TEXT("[UExMatchFSM] 무시됨: 안전 전이가 대기 중인데 일반 전이가 요청됨. (Req=%d)"), (int32)NewState);
				return;
			}
		}
		
		PendingMatchStateTransition = TPair<EExMatchState, ETransitionReason>(NewState, Reason);
		UE_LOG(LogExNetwork, Verbose, TEXT("[UExMatchFSM] 재진입 전이 대기열 등록 (Req=%d, Reason=%d)"), (int32)NewState, (int32)Reason);
		return;
	}

	if (CurrentMatchState == NewState)
	{
		return; // 이미 같은 상태
	}

	if (!IsTransitionAllowed(CurrentMatchState, NewState))
	{
		UE_LOG(LogExNetwork, Error, TEXT("[UExMatchFSM] 잘못된 상태 전이 시도: %d -> %d"), (int32)CurrentMatchState, (int32)NewState);
		return;
	}

	bIsTransitioningMatchState = true;

	EExMatchState OldState = CurrentMatchState;
	
	UE_LOG(LogExNetwork, Log, TEXT("[ExMatchFSM] [%d -> %d] reason=%d"), (int32)OldState, (int32)NewState, (int32)Reason);

	HandleExitMatchState(OldState);
	CurrentMatchState = NewState;
	HandleEnterMatchState(NewState);

	bIsTransitioningMatchState = false;

	OnMatchStateChanged.Broadcast(OldState, NewState);

	// 펜딩 처리
	if (PendingMatchStateTransition.IsSet() && !PendingMatchStateTickerHandle.IsValid())
	{
		PendingMatchStateTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &UExOnlineSubsystem::TickPendingMatchState), 0.0f
		);
	}
}

bool UExOnlineSubsystem::TickPendingMatchState(float DeltaTime)
{
	PendingMatchStateTickerHandle.Reset();

	if (PendingMatchStateTransition.IsSet())
	{
		TPair<EExMatchState, ETransitionReason> Pending = PendingMatchStateTransition.GetValue();
		PendingMatchStateTransition.Reset();
		
		UE_LOG(LogExNetwork, Log, TEXT("[UExMatchFSM] 지연된 전이 실행."));
		TransitionMatchState(Pending.Key, Pending.Value);
	}
	return false; // 일회성
}

void UExOnlineSubsystem::HandleEnterMatchState(EExMatchState NewState)
{
	switch (NewState)
	{
	case EExMatchState::Idle:
		if (ServerStrategy)
		{
			ServerStrategy->ResetTransientState();
			ServerStrategy->CancelMatch();
		}
		// Idle 진입 시 실패/취소 에러 브로드캐스트 (빈 문자열이 아니면 실패로 간주)
		if (!LastErrorMessage.IsEmpty())
		{
			OnMatchFound.Broadcast(false, LastErrorMessage);
			LastErrorMessage.Empty();
		}
		break;

	case EExMatchState::Searching:
		if (ServerStrategy)
		{
			ServerStrategy->BeginSearchPhase(PendingMatchConfig, EExMatchState::Searching,
				[this](bool bSuccess, const FString& ErrorMessage)
				{
					if (!ShouldHonorCallback(EExMatchState::Searching)) return;

					if (bSuccess)
					{
						TransitionMatchState(EExMatchState::Joining, ETransitionReason::AsyncCallback);
					}
					else if (ErrorMessage == TEXT("Timeout"))
					{
						TransitionMatchState(EExMatchState::Creating, ETransitionReason::Timeout);
					}
					else
					{
						LastErrorMessage = ErrorMessage;
						TransitionMatchState(EExMatchState::Idle, ETransitionReason::AsyncCallback);
					}
				});
		}
		break;

	case EExMatchState::Creating:
		if (ServerStrategy)
		{
			ServerStrategy->BeginCreatePhase(PendingMatchConfig, EExMatchState::Creating,
				[this](bool bSuccess, const FString& ErrorMessage)
				{
					if (!ShouldHonorCallback(EExMatchState::Creating)) return;

					if (bSuccess)
					{
						TransitionMatchState(EExMatchState::Waiting, ETransitionReason::AsyncCallback);
					}
					else
					{
						LastErrorMessage = ErrorMessage;
						TransitionMatchState(EExMatchState::Idle, ETransitionReason::AsyncCallback);
					}
				});
		}
		break;

	case EExMatchState::Joining:
		if (ServerStrategy)
		{
			// SessionId는 LobbyIndex 용도로 "0"을 전달 (Phase B 내부 구현에 맡김)
			ServerStrategy->BeginJoinPhase(PendingMatchConfig, TEXT("0"), EExMatchState::Joining,
				[this](bool bSuccess, const FString& ErrorMessage)
				{
					if (!ShouldHonorCallback(EExMatchState::Joining)) return;

					if (bSuccess)
					{
						TransitionMatchState(EExMatchState::Waiting, ETransitionReason::AsyncCallback);
					}
					else
					{
						LastErrorMessage = ErrorMessage;
						TransitionMatchState(EExMatchState::Idle, ETransitionReason::AsyncCallback);
					}
				});
		}
		break;

	case EExMatchState::Waiting:
		if (ServerStrategy)
		{
			ServerStrategy->BeginWaitPhase(PendingMatchConfig, ServerStrategy->IsHost(), EExMatchState::Waiting,
				[this](bool bSuccess, const FString& ErrorMessage)
				{
					if (!ShouldHonorCallback(EExMatchState::Waiting)) return;

					if (bSuccess)
					{
						TransitionMatchState(EExMatchState::Ready, ETransitionReason::AsyncCallback);
					}
					else
					{
						LastErrorMessage = ErrorMessage;
						TransitionMatchState(EExMatchState::Idle, ETransitionReason::AsyncCallback);
					}
				});
		}
		break;

	case EExMatchState::Ready:
		UE_LOG(LogExNetwork, Log, TEXT("[UExMatchFSM] 매칭 완료 — 성공 (Ready 진입)."));
		OnMatchFound.Broadcast(true, TEXT(""));
		break;

	case EExMatchState::InGame:
		// StartGame에서 처리됨
		break;
	}
}

void UExOnlineSubsystem::HandleExitMatchState(EExMatchState OldState)
{
	switch (OldState)
	{
	case EExMatchState::Searching:
		if (ServerStrategy) ServerStrategy->EndSearchPhase();
		break;
	case EExMatchState::Creating:
		if (ServerStrategy) ServerStrategy->EndCreatePhase();
		break;
	case EExMatchState::Joining:
		if (ServerStrategy) ServerStrategy->EndJoinPhase();
		break;
	case EExMatchState::Waiting:
		if (ServerStrategy) ServerStrategy->EndWaitPhase();
		break;
	default:
		break;
	}
}
