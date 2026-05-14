// Copyright ExFrameWork. All Rights Reserved.

#include "ExEOSAuthProvider.h"
#include "Core/ExNetworkLog.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "IEOSSDKManager.h"

#if WITH_EOS_SDK
#include "eos_sdk.h"
#include "eos_connect.h"
#endif

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

#if WITH_EOS_SDK
	if (IEOSSDKManager* SDKManager = IEOSSDKManager::Get())
	{
		auto Platforms = SDKManager->GetActivePlatforms();
		if (Platforms.Num() > 0)
		{
			EOS_HPlatform Platform = *Platforms[0];
			EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(Platform);

			EOS_Connect_CreateDeviceIdOptions Options = {};
			Options.ApiVersion = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
			Options.DeviceModel = "PC";

			struct FCreateDeviceContext
			{
				FExEOSAuthProvider* Provider;
				IOnlineIdentityPtr Identity;
				int32 LocalUserNum;
			};

			FCreateDeviceContext* Context = new FCreateDeviceContext{ this, Identity, LocalUserNum };

			UE_LOG(LogExNetwork, Log, TEXT("[ExEOSAuthProvider] EOS_Connect_CreateDeviceId 호출 시작..."));
			
			EOS_Connect_CreateDeviceId(ConnectHandle, &Options, Context, [](const EOS_Connect_CreateDeviceIdCallbackInfo* Data)
			{
				FCreateDeviceContext* Ctx = static_cast<FCreateDeviceContext*>(Data->ClientData);
				
				if (Data->ResultCode == EOS_EResult::EOS_Success || Data->ResultCode == EOS_EResult::EOS_DuplicateNotAllowed)
				{
					if (Data->ResultCode == EOS_EResult::EOS_Success)
					{
						UE_LOG(LogExNetwork, Log, TEXT("[ExEOSAuthProvider] CreateDeviceId 성공."));
					}
					else
					{
						UE_LOG(LogExNetwork, Log, TEXT("[ExEOSAuthProvider] CreateDeviceId 중복/이미 존재함."));
					}

					FOnlineAccountCredentials Credentials;
					Credentials.Type = TEXT("externalauth:DeviceIdAccessToken");
					Credentials.Id = TEXT("");
					Credentials.Token = TEXT("");

					UE_LOG(LogExNetwork, Log, TEXT("[ExEOSAuthProvider] Identity->Login() 호출."));
					Ctx->Identity->Login(Ctx->LocalUserNum, Credentials);
				}
				else
				{
					FString ErrorMsg = FString::Printf(TEXT("CreateDeviceId failed: %d"), (int32)Data->ResultCode);
					UE_LOG(LogExNetwork, Error, TEXT("[ExEOSAuthProvider] %s"), *ErrorMsg);
					Ctx->Provider->OnLoginComplete.Broadcast(false, ErrorMsg);
				}
				delete Ctx;
			});

			return; // 콜백에서 Login을 이어서 수행함
		}
	}
#endif

	UE_LOG(LogExNetwork, Warning, TEXT("[ExEOSAuthProvider] EOS SDK를 찾을 수 없습니다. 다이렉트 로그인 시도."));

	// Fallback if SDK or Platform is not available
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
