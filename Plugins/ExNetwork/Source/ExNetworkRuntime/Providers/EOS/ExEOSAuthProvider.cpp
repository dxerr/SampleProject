// Copyright ExFrameWork. All Rights Reserved.

#include "ExEOSAuthProvider.h"
#include "Core/ExNetworkLog.h"
#include "OnlineSubsystem.h"
#include "IOnlineSubsystemEOS.h"
#include "IEOSSDKManager.h"

#if WITH_EOS_SDK
#include "eos_connect.h"
#include "eos_platform.h"

FExEOSAuthProvider::FExEOSAuthProvider()
{
	UE_LOG(LogExNetwork, Log, TEXT("[ExEOSAuthProvider] 생성됨 — EOS Connect Device ID 직접 호출 방식."));
}

void FExEOSAuthProvider::Login(int32 LocalUserNum)
{
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get(TEXT("EOS"));
	IOnlineSubsystemEOS* EOSOSS = static_cast<IOnlineSubsystemEOS*>(OSS);
	if (!ensureMsgf(EOSOSS, TEXT("[ExEOSAuthProvider] IOnlineSubsystemEOS 캐스트 실패.")))
	{
		OnLoginComplete.Broadcast(false, TEXT("EOS OSS cast failed"));
		return;
	}

	IEOSPlatformHandlePtr PlatformHandle = EOSOSS->GetEOSPlatformHandle();
	if (!ensureMsgf(PlatformHandle.IsValid(), TEXT("[ExEOSAuthProvider] EOS Platform Handle 없음.")))
	{
		OnLoginComplete.Broadcast(false, TEXT("EOS Platform Handle invalid"));
		return;
	}

	EOS_HPlatform EOSPlatform = *PlatformHandle;
	EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(EOSPlatform);

	UE_LOG(LogExNetwork, Log, TEXT("[ExEOSAuthProvider] Login 시작 — LocalUserNum=%d, DeviceID 방식."), LocalUserNum);

	EOS_Connect_CreateDeviceIdOptions CreateOptions = {};
	CreateOptions.ApiVersion = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
	CreateOptions.DeviceModel = TCHAR_TO_UTF8(TEXT("PC"));

	struct FCreateDeviceIdContext { FExEOSAuthProvider* Provider; int32 LocalUserNum; };
	FCreateDeviceIdContext* Context = new FCreateDeviceIdContext{ this, LocalUserNum };

	EOS_Connect_CreateDeviceId(ConnectHandle, &CreateOptions, Context,
		[](const EOS_Connect_CreateDeviceIdCallbackInfo* Data)
		{
			auto* Ctx = static_cast<FCreateDeviceIdContext*>(Data->ClientData);
			Ctx->Provider->OnCreateDeviceIdComplete(Data, Ctx->LocalUserNum);
			delete Ctx;
		}
	);
}

void FExEOSAuthProvider::OnCreateDeviceIdComplete(const EOS_Connect_CreateDeviceIdCallbackInfo* Data, int32 LocalUserNum)
{
	if (Data->ResultCode != EOS_EResult::EOS_Success && Data->ResultCode != EOS_EResult::EOS_DuplicateNotAllowed)
	{
		const FString ErrStr(EOS_EResult_ToString(Data->ResultCode));
		UE_LOG(LogExNetwork, Warning, TEXT("[ExEOSAuthProvider] CreateDeviceId 실패 — %s"), *ErrStr);
		OnLoginComplete.Broadcast(false, ErrStr);
		return;
	}

	UE_LOG(LogExNetwork, Log, TEXT("[ExEOSAuthProvider] DeviceId 준비 완료 (%s) — Connect Login 시작."),
		UTF8_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));

	IOnlineSubsystem* OSS = IOnlineSubsystem::Get(TEXT("EOS"));
	IOnlineSubsystemEOS* EOSOSS = static_cast<IOnlineSubsystemEOS*>(OSS);
	if (!EOSOSS) { OnLoginComplete.Broadcast(false, TEXT("EOS OSS lost")); return; }

	IEOSPlatformHandlePtr PlatformHandle = EOSOSS->GetEOSPlatformHandle();
	if (!PlatformHandle.IsValid()) { OnLoginComplete.Broadcast(false, TEXT("Platform Handle lost")); return; }

	EOS_HPlatform EOSPlatform = *PlatformHandle;
	EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(EOSPlatform);

	EOS_Connect_Credentials Credentials = {};
	Credentials.ApiVersion = EOS_CONNECT_CREDENTIALS_API_LATEST;
	Credentials.Token = nullptr;
	Credentials.Type = EOS_EExternalCredentialType::EOS_ECT_DEVICEID_ACCESS_TOKEN;

	EOS_Connect_UserLoginInfo UserLoginInfo = {};
	UserLoginInfo.ApiVersion = EOS_CONNECT_USERLOGININFO_API_LATEST;
	UserLoginInfo.DisplayName = TCHAR_TO_UTF8(TEXT("Player"));

	EOS_Connect_LoginOptions LoginOptions = {};
	LoginOptions.ApiVersion = EOS_CONNECT_LOGIN_API_LATEST;
	LoginOptions.Credentials = &Credentials;
	LoginOptions.UserLoginInfo = &UserLoginInfo;

	struct FConnectLoginContext { FExEOSAuthProvider* Provider; int32 LocalUserNum; };
	FConnectLoginContext* Context = new FConnectLoginContext{ this, LocalUserNum };

	EOS_Connect_Login(ConnectHandle, &LoginOptions, Context,
		[](const EOS_Connect_LoginCallbackInfo* Data)
		{
			auto* Ctx = static_cast<FConnectLoginContext*>(Data->ClientData);
			Ctx->Provider->OnConnectLoginComplete(Data, Ctx->LocalUserNum);
			delete Ctx;
		}
	);
}

void FExEOSAuthProvider::OnConnectLoginComplete(const EOS_Connect_LoginCallbackInfo* Data, int32 LocalUserNum)
{
	if (Data->ResultCode == EOS_EResult::EOS_Success)
	{
		bLoggedIn = true;
		UE_LOG(LogExNetwork, Log, TEXT("[ExEOSAuthProvider] Connect Login 성공 — LocalUserNum=%d"), LocalUserNum);
		OnLoginComplete.Broadcast(true, TEXT(""));
	}
	else if (Data->ResultCode == EOS_EResult::EOS_InvalidUser && Data->ContinuanceToken != nullptr)
	{
		// 최초 로그인 — 신규 유저 생성 필요
		UE_LOG(LogExNetwork, Log, TEXT("[ExEOSAuthProvider] 신규 유저 — CreateUser 시도."));

		IOnlineSubsystem* OSS = IOnlineSubsystem::Get(TEXT("EOS"));
		IOnlineSubsystemEOS* EOSOSS = static_cast<IOnlineSubsystemEOS*>(OSS);
		if (!EOSOSS) { OnLoginComplete.Broadcast(false, TEXT("EOS OSS lost")); return; }

		IEOSPlatformHandlePtr PlatformHandle = EOSOSS->GetEOSPlatformHandle();
		if (!PlatformHandle.IsValid()) { OnLoginComplete.Broadcast(false, TEXT("Platform Handle lost")); return; }

		EOS_HPlatform EOSPlatform = *PlatformHandle;
		EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(EOSPlatform);

		EOS_Connect_CreateUserOptions CreateUserOptions = {};
		CreateUserOptions.ApiVersion = EOS_CONNECT_CREATEUSER_API_LATEST;
		CreateUserOptions.ContinuanceToken = Data->ContinuanceToken;

		struct FCreateUserContext { FExEOSAuthProvider* Provider; int32 LocalUserNum; };
		FCreateUserContext* Context = new FCreateUserContext{ this, LocalUserNum };

		EOS_Connect_CreateUser(ConnectHandle, &CreateUserOptions, Context,
			[](const EOS_Connect_CreateUserCallbackInfo* Data)
			{
				auto* Ctx = static_cast<FCreateUserContext*>(Data->ClientData);
				if (Data->ResultCode == EOS_EResult::EOS_Success)
				{
					Ctx->Provider->bLoggedIn = true;
					UE_LOG(LogExNetwork, Log, TEXT("[ExEOSAuthProvider] CreateUser 성공."));
					Ctx->Provider->OnLoginComplete.Broadcast(true, TEXT(""));
				}
				else
				{
					const FString ErrStr(EOS_EResult_ToString(Data->ResultCode));
					UE_LOG(LogExNetwork, Warning, TEXT("[ExEOSAuthProvider] CreateUser 실패 — %s"), *ErrStr);
					Ctx->Provider->OnLoginComplete.Broadcast(false, ErrStr);
				}
				delete Ctx;
			}
		);
	}
	else
	{
		const FString ErrStr(EOS_EResult_ToString(Data->ResultCode));
		UE_LOG(LogExNetwork, Warning, TEXT("[ExEOSAuthProvider] Connect Login 실패 — %s"), *ErrStr);
		OnLoginComplete.Broadcast(false, ErrStr);
	}
}

void FExEOSAuthProvider::Logout(int32 LocalUserNum)
{
	bLoggedIn = false;
	UE_LOG(LogExNetwork, Log, TEXT("[ExEOSAuthProvider] Logout — LocalUserNum=%d"), LocalUserNum);
	OnLogoutComplete.Broadcast(true);
}

bool FExEOSAuthProvider::IsLoggedIn(int32 LocalUserNum) const
{
	return bLoggedIn;
}

#else // !WITH_EOS_SDK

// EOS SDK 미지원 환경에서는 빈 구현 제공
FExEOSAuthProvider::FExEOSAuthProvider()
{
	UE_LOG(LogExNetwork, Warning, TEXT("[ExEOSAuthProvider] EOS SDK 미지원 환경 — 로그인 불가."));
}
void FExEOSAuthProvider::Login(int32 LocalUserNum)
{
	OnLoginComplete.Broadcast(false, TEXT("EOS SDK not available"));
}
void FExEOSAuthProvider::Logout(int32 LocalUserNum)
{
	OnLogoutComplete.Broadcast(false);
}
bool FExEOSAuthProvider::IsLoggedIn(int32 LocalUserNum) const { return false; }

#endif // WITH_EOS_SDK
