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
#include "OnlineDelegates.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

void UExOnlineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] Initialize 시작."));

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

	IOnlineSubsystem* OSS = IOnlineSubsystem::Get(TEXT("EOS"));
	if (OSS && OSS->GetSubsystemName() != FName("NULL") && !AuthProvider)
	{
		UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] EOS OSS 즉시 획득 — 직접 초기화 진행."));
		FOnlineSubsystemDelegates::OnOnlineSubsystemCreated.Remove(SubsystemCreatedHandle);
		SubsystemCreatedHandle.Reset();
		InitAuthProviderAndLogin(OSS);
	}
}

IOnlineSubsystem* UExOnlineSubsystem::TryGetEOSSubsystem() const
{
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get(TEXT("EOS"));
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

	UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] OnOnlineSubsystemCreated 콜백 — EOS OSS 생성 확인."));
	FOnlineSubsystemDelegates::OnOnlineSubsystemCreated.Remove(SubsystemCreatedHandle);
	SubsystemCreatedHandle.Reset();
	InitAuthProviderAndLogin(NewSubsystem);
}

void UExOnlineSubsystem::InitAuthProviderAndLogin(IOnlineSubsystem* OSS)
{
	check(OSS);
	UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] OSS 확정 — 서브시스템명: %s"), *OSS->GetSubsystemName().ToString());

	AuthProvider = MakeUnique<FExEOSAuthProvider>();
	UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] AuthProvider 선택: EOS"));

	if (FExListenServerStrategy* ListenStrategy = static_cast<FExListenServerStrategy*>(ServerStrategy.Get()))
	{
		if (ListenStrategy->GetServerType() == EExServerType::ListenServer)
		{
			ListenStrategy->SetLobbyProvider(MakeUnique<FExEOSLobbyProvider>(OSS));
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

	if (SubsystemCreatedHandle.IsValid())
	{
		FOnlineSubsystemDelegates::OnOnlineSubsystemCreated.Remove(SubsystemCreatedHandle);
		SubsystemCreatedHandle.Reset();
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
	UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] StartGame — MapPath=%s"), *Config.MapPath);
	OnGameStarted.Broadcast(true, TEXT(""));

	if (ListenStrategy->IsHost())
	{
		// 호스트: 방 생성자이므로 ServerTravel을 통해 게임 맵으로 이동
		ListenStrategy->StartGameSession(Config.MapPath, World);
	}
	else
	{
		// 클라이언트: 로비에 참가한 입장이므로 호스트 세션으로 ClientTravel 수행
		FString ConnectString = ListenStrategy->GetConnectString();
		UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] StartGame: 클라이언트 — ClientTravel 실행 URL=%s"), *ConnectString);
		World->GetFirstPlayerController()->ClientTravel(ConnectString, TRAVEL_Absolute);
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
