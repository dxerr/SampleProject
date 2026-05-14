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
		UE_LOG(LogExNetwork, Warning, TEXT("[UExOnlineSubsystem] FindQuickMatch: 이미 매칭 진행 중. State=%d"), (int32)CurrentMatchState);
		OnMatchFound.Broadcast(false, TEXT("Already matching"));
		return;
	}

	FExListenServerStrategy* ListenStrategy = static_cast<FExListenServerStrategy*>(ServerStrategy.Get());
	if (!ensureMsgf(ListenStrategy, TEXT("[UExOnlineSubsystem] FindQuickMatch: ListenServerStrategy 없음.")))
	{
		OnMatchFound.Broadcast(false, TEXT("Strategy not available"));
		return;
	}

	CurrentMatchState = EExMatchState::Searching;
	UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] FindQuickMatch 시작 — MatchMode=%s"), *Config.MatchMode);

	ListenStrategy->FindAndJoinOrCreate(Config,
		[this](bool bSuccess, const FString& ErrorMessage)
		{
			CurrentMatchState = bSuccess ? EExMatchState::Ready : EExMatchState::Idle;
			if (bSuccess)
			{
				UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] 매칭 완료 — 성공."));
			}
			else
			{
				UE_LOG(LogExNetwork, Warning, TEXT("[UExOnlineSubsystem] 매칭 실패 — %s"), *ErrorMessage);
			}
			OnMatchFound.Broadcast(bSuccess, ErrorMessage);
		}
	);
}

void UExOnlineSubsystem::CancelMatch()
{
	FExListenServerStrategy* ListenStrategy = static_cast<FExListenServerStrategy*>(ServerStrategy.Get());
	if (ListenStrategy)
	{
		ListenStrategy->CancelMatch();
	}
	CurrentMatchState = EExMatchState::Idle;
	UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] CancelMatch 완료."));
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

	// 클라이언트는 StartGame 호출 무시 (ServerTravel은 서버만)
	if (World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] StartGame: 클라이언트 — 서버의 ServerTravel을 기다립니다."));
		return;
	}

	if (!ensureMsgf(ServerStrategy, TEXT("[UExOnlineSubsystem] StartGame: ServerStrategy 없음.")))
	{
		OnGameStarted.Broadcast(false, TEXT("ServerStrategy not available"));
		return;
	}

	CurrentMatchState = EExMatchState::InGame;
	UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] StartGame — MapPath=%s"), *Config.MapPath);

	OnGameStarted.Broadcast(true, TEXT(""));
	ServerStrategy->StartGameSession(Config.MapPath, World);
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
