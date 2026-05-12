// Copyright ExFrameWork. All Rights Reserved.

#include "ExOnlineSubsystem.h"
#include "ExNetworkLog.h"
#include "IExAuthProvider.h"
#include "IExNetServerStrategy.h"
#include "Providers/EOS/ExEOSAuthProvider.h"
#include "Providers/Null/ExNullAuthProvider.h"
#include "Strategies/ExListenServerStrategy.h"
#include "Strategies/ExDedicatedServerStrategy.h"
#include "OnlineSubsystem.h"
#include "OnlineDelegates.h"
#include "Engine/World.h"

void UExOnlineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] Initialize 시작."));

	// ------------------------------------------------------------------
	// 1. 서버 Strategy 자동 선택
	// ------------------------------------------------------------------
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

	// ------------------------------------------------------------------
	// 2. EOS OSS 획득
	//
	// OnOnlineSubsystemCreated 는 IOnlineSubsystem::Get() 이 처음 호출될 때
	// 내부에서 생성 후 브로드캐스트된다. 즉 누군가 Get()을 호출해야 생성된다.
	//
	// 처리 순서:
	//   (1) 먼저 구독 등록
	//   (2) Get(TEXT("EOS")) 직접 호출하여 생성 유도
	//       → 이미 생성되어 있으면 Get()이 기존 인스턴스를 즉시 반환
	//       → 미생성이면 Get() 내부에서 생성 후 OnOnlineSubsystemCreated 브로드캐스트
	//   (3) Get()이 유효한 EOS OSS를 반환하면 즉시 로그인 진행
	//       브로드캐스트가 구독 직후 동기로 발생했을 경우 HandleOnlineSubsystemCreated
	//       에서 이미 처리되었으므로 중복 처리 방지를 위해 AuthProvider 생성 여부 확인
	// ------------------------------------------------------------------

	// (1) 구독 먼저 등록
	SubsystemCreatedHandle = FOnlineSubsystemDelegates::OnOnlineSubsystemCreated.AddUObject(
		this, &UExOnlineSubsystem::HandleOnlineSubsystemCreated
	);

	// (2) Get() 호출로 생성 유도 — 동기 브로드캐스트 발생 가능
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get(TEXT("EOS"));

	// (3) Get()이 반환했지만 콜백이 동기 처리되지 않은 경우 직접 처리
	if (OSS && OSS->GetSubsystemName() == FName("EOS") && !AuthProvider)
	{
		UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] EOS OSS 즉시 획득 — 직접 초기화 진행."));

		// 구독 해제 (더 이상 불필요)
		FOnlineSubsystemDelegates::OnOnlineSubsystemCreated.Remove(SubsystemCreatedHandle);
		SubsystemCreatedHandle.Reset();

		InitAuthProviderAndLogin(OSS);
	}
	else if ((!OSS || OSS->GetSubsystemName() == FName("NULL")) && !AuthProvider)
	{
		UE_LOG(LogExNetwork, Error, TEXT("[UExOnlineSubsystem] EOS OSS 획득 실패. NullAuthProvider로 폴백합니다."));

		// 구독 해제
		FOnlineSubsystemDelegates::OnOnlineSubsystemCreated.Remove(SubsystemCreatedHandle);
		SubsystemCreatedHandle.Reset();

		AuthProvider = MakeUnique<FExNullAuthProvider>();
		UE_LOG(LogExNetwork, Log, TEXT("[UExOnlineSubsystem] AuthProvider 선택: Null"));

		AuthProvider->OnLoginComplete.AddUObject(this, &UExOnlineSubsystem::HandleAuthLoginComplete);
		AuthProvider->Login(0);
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

	// 구독 해제 (1회만 처리)
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
	if (!ServerStrategy)
	{
		return TEXT("None");
	}
	return (ServerStrategy->GetServerType() == EExServerType::ListenServer)
		? TEXT("ListenServer")
		: TEXT("DedicatedServer");
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
