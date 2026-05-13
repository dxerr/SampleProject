// Copyright ExFrameWork. All Rights Reserved.

#include "ExEOSAuthProvider.h"
#include "Core/ExNetworkLog.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"

FExEOSAuthProvider::FExEOSAuthProvider()
{
	UE_LOG(LogExNetwork, Log, TEXT("[ExEOSAuthProvider] 생성됨 — IOnlineIdentity::Login() EOS Connect Device ID 방식."));
}

FExEOSAuthProvider::~FExEOSAuthProvider()
{
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get(TEXT("EOS"));
	if (OSS)
	{
		IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
		if (Identity.IsValid())
		{
			Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginCompleteHandle);
			Identity->ClearOnLogoutCompleteDelegate_Handle(0, LogoutCompleteHandle);
		}
	}
}

void FExEOSAuthProvider::Login(int32 LocalUserNum)
{
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get(TEXT("EOS"));
	if (!ensureMsgf(OSS, TEXT("[ExEOSAuthProvider] EOS OSS 없음.")))
	{
		OnLoginComplete.Broadcast(false, TEXT("EOS OSS not found"));
		return;
	}

	IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
	if (!ensureMsgf(Identity.IsValid(), TEXT("[ExEOSAuthProvider] IOnlineIdentity 없음.")))
	{
		OnLoginComplete.Broadcast(false, TEXT("Identity interface not available"));
		return;
	}

	UE_LOG(LogExNetwork, Log, TEXT("[ExEOSAuthProvider] Login 시작 — LocalUserNum=%d"), LocalUserNum);

	LoginCompleteHandle = Identity->AddOnLoginCompleteDelegate_Handle(
		LocalUserNum,
		FOnLoginCompleteDelegate::CreateRaw(this, &FExEOSAuthProvider::HandleLoginComplete)
	);

	// FUserManagerEOS::CallEOSConnectLogin()은 Credentials.Type을 "externalauth:TYPE" 형식으로 파싱한다.
	// LexFromString(EOS_EExternalCredentialType) 기준 Device ID 문자열 = "DeviceIdAccessToken"
	// Token은 빈 문자열 — Device ID는 EOS SDK가 자동 관리한다.
	FOnlineAccountCredentials Credentials;
	Credentials.Type = TEXT("externalauth:DeviceIdAccessToken");
	Credentials.Id = TEXT("");
	Credentials.Token = TEXT("");

	Identity->Login(LocalUserNum, Credentials);
}

void FExEOSAuthProvider::Logout(int32 LocalUserNum)
{
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get(TEXT("EOS"));
	if (!OSS)
	{
		OnLogoutComplete.Broadcast(false);
		return;
	}

	IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
	if (!Identity.IsValid())
	{
		OnLogoutComplete.Broadcast(false);
		return;
	}

	LogoutCompleteHandle = Identity->AddOnLogoutCompleteDelegate_Handle(
		LocalUserNum,
		FOnLogoutCompleteDelegate::CreateRaw(this, &FExEOSAuthProvider::HandleLogoutComplete)
	);

	Identity->Logout(LocalUserNum);
}

bool FExEOSAuthProvider::IsLoggedIn(int32 LocalUserNum) const
{
	return bLoggedIn;
}

void FExEOSAuthProvider::HandleLoginComplete(int32 LocalUserNum, bool bSuccess, const FUniqueNetId& UserId, const FString& ErrorStr)
{
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get(TEXT("EOS"));
	if (OSS)
	{
		IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
		if (Identity.IsValid())
		{
			Identity->ClearOnLoginCompleteDelegate_Handle(LocalUserNum, LoginCompleteHandle);
		}
	}

	bLoggedIn = bSuccess;

	if (bSuccess)
	{
		UE_LOG(LogExNetwork, Log, TEXT("[ExEOSAuthProvider] Login 완료 — 성공. LocalUserNum=%d"), LocalUserNum);
	}
	else
	{
		UE_LOG(LogExNetwork, Warning, TEXT("[ExEOSAuthProvider] Login 실패 — LocalUserNum=%d, Error=%s"), LocalUserNum, *ErrorStr);
	}

	OnLoginComplete.Broadcast(bSuccess, ErrorStr);
}

void FExEOSAuthProvider::HandleLogoutComplete(int32 LocalUserNum, bool bSuccess)
{
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get(TEXT("EOS"));
	if (OSS)
	{
		IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
		if (Identity.IsValid())
		{
			Identity->ClearOnLogoutCompleteDelegate_Handle(LocalUserNum, LogoutCompleteHandle);
		}
	}

	bLoggedIn = false;
	UE_LOG(LogExNetwork, Log, TEXT("[ExEOSAuthProvider] Logout 완료 — LocalUserNum=%d"), LocalUserNum);
	OnLogoutComplete.Broadcast(bSuccess);
}
