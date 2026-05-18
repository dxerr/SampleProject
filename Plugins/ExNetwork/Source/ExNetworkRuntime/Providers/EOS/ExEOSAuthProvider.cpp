// Copyright ExFrameWork. All Rights Reserved.

#include "ExEOSAuthProvider.h"
#include "Core/ExNetworkLog.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "IEOSSDKManager.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"

#if WITH_EOS_SDK
#include "eos_sdk.h"
#include "eos_connect.h"
#endif

FExEOSAuthProvider::FExEOSAuthProvider(IOnlineSubsystem* InOSS)
	: OSS(InOSS)
{
	ensureMsgf(OSS != nullptr, TEXT("[ExEOSAuthProvider] 생성 시 OSS가 nullptr입니다."));
	UE_LOG(LogExNetwork, Log, TEXT("[ExEOSAuthProvider] 생성됨 — IOnlineIdentity::Login() EOS Connect Device ID 방식."));
}

FExEOSAuthProvider::~FExEOSAuthProvider()
{
	if (OSS)
	{
		IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
		if (Identity.IsValid())
		{
			Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginCompleteHandle);
			Identity->ClearOnLogoutCompleteDelegate_Handle(0, LogoutCompleteHandle);
		}
	}

	// PUID 검증 Ticker 정리
	if (PuidValidationTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PuidValidationTickerHandle);
		PuidValidationTickerHandle.Reset();
	}
}

void FExEOSAuthProvider::Login(int32 LocalUserNum)
{
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

	if (bSuccess)
	{
		// 성공 — PUID 매핑 대기 폴링 시작 (즉시 Broadcast 안 함)
		StartPuidValidationPolling(LocalUserNum, ErrorStr);
	}
	else
	{
		// 실패 — 즉시 Broadcast
		OnLoginComplete.Broadcast(false, ErrorStr);
	}
}

void FExEOSAuthProvider::StartPuidValidationPolling(int32 LocalUserNum, const FString& CachedErrorStr)
{
	// 이미 폴링 중이면 기존 Ticker 정리
	if (PuidValidationTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PuidValidationTickerHandle);
		PuidValidationTickerHandle.Reset();
	}

	PuidValidationStartTime = FPlatformTime::Seconds();
	UE_LOG(LogExNetwork, Log, TEXT("[ExEOSAuthProvider] PUID 매핑 폴링 시작 ? LocalUserNum=%d, MaxWait=%.1fs"),
		LocalUserNum, MaxPuidValidationSeconds);

	// 즉시 한 번 확인 (이미 매핑됐을 수도 있음)
	if (IsLocalPuidValid(LocalUserNum))
	{
		UE_LOG(LogExNetwork, Log, TEXT("[ExEOSAuthProvider] PUID 즉시 매핑 확인됨 ? LocalUserNum=%d"), LocalUserNum);
		OnLoginComplete.Broadcast(true, CachedErrorStr);
		return;
	}

	PuidValidationTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FExEOSAuthProvider::TickPuidValidation, LocalUserNum, CachedErrorStr),
		PuidValidationPollInterval);
}

bool FExEOSAuthProvider::IsLocalPuidValid(int32 LocalUserNum) const
{
	if (!OSS) return false;

	IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
	if (!Identity.IsValid()) return false;

	TSharedPtr<const FUniqueNetId> Id = Identity->GetUniquePlayerId(LocalUserNum);
	if (!Id.IsValid() || !Id->IsValid()) return false;

	// EOS UniqueNetId 형식: "<EpicAccountId>|<ProductUserId>" 또는 "|<ProductUserId>"
	const FString Str = Id->ToString();
	int32 PipeIdx = INDEX_NONE;
	if (!Str.FindChar(TEXT('|'), PipeIdx)) return false;

	const FString PuidPart = Str.RightChop(PipeIdx + 1);
	// EOS ProductUserId는 32자 hex
	return PuidPart.Len() >= 16;
}

bool FExEOSAuthProvider::TickPuidValidation(float DeltaTime, int32 LocalUserNum, FString CachedErrorStr)
{
	const double Elapsed = FPlatformTime::Seconds() - PuidValidationStartTime;

	if (IsLocalPuidValid(LocalUserNum))
	{
		UE_LOG(LogExNetwork, Log, TEXT("[ExEOSAuthProvider] PUID 매핑 확인됨 ? LocalUserNum=%d, 경과=%.2fs"),
			LocalUserNum, Elapsed);
		PuidValidationTickerHandle.Reset();
		OnLoginComplete.Broadcast(true, CachedErrorStr);
		return false; // Ticker 종료
	}

	if (Elapsed >= MaxPuidValidationSeconds)
	{
		UE_LOG(LogExNetwork, Warning, TEXT("[ExEOSAuthProvider] PUID 매핑 타임아웃 ? LocalUserNum=%d, 경과=%.2fs"),
			LocalUserNum, Elapsed);
		PuidValidationTickerHandle.Reset();
		bLoggedIn = false;
		OnLoginComplete.Broadcast(false, TEXT("PUID validation timeout"));
		return false;
	}

	return true; // 계속 폴링
}

void FExEOSAuthProvider::HandleLogoutComplete(int32 LocalUserNum, bool bSuccess)
{
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
