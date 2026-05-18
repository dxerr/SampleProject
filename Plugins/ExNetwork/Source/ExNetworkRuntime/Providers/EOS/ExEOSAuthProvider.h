// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/IExAuthProvider.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Containers/Ticker.h"

/**
 * FExEOSAuthProvider
 *
 * IExAuthProvider의 EOS 구현체.
 * IOnlineIdentity::AutoLogin() 을 통해 EOS Connect Device ID 로그인을 수행한다.
 *
 * bUseEAS=false, bUseEOSConnect=true 설정 하에서 AutoLogin은
 * FUserManagerEOS::CallEOSConnectLogin() → EOS Connect Device ID 경로를 타며,
 * IOnlineIdentity의 LocalUser 등록까지 처리하여 IOnlineSession(Lobby/Session) 사용이 가능해진다.
 *
 * 이전에 EOS SDK 직접 호출(EOS_Connect_Login) 방식은 EOS Connect 레이어만 로그인되고
 * IOnlineIdentity LocalUser가 등록되지 않아 IOnlineSession에서 "user not logged in" 에러가 발생했다.
 */
class IOnlineSubsystem;

class FExEOSAuthProvider : public IExAuthProvider
{
public:

	explicit FExEOSAuthProvider(IOnlineSubsystem* InOSS);
	virtual ~FExEOSAuthProvider() override;

	virtual void Login(int32 LocalUserNum) override;
	virtual void Logout(int32 LocalUserNum) override;
	virtual bool IsLoggedIn(int32 LocalUserNum) const override;

private:

	void HandleLoginComplete(int32 LocalUserNum, bool bSuccess, const FUniqueNetId& UserId, const FString& ErrorStr);
	void HandleLogoutComplete(int32 LocalUserNum, bool bSuccess);

	IOnlineSubsystem* OSS = nullptr;

	bool bLoggedIn = false;

	FDelegateHandle LoginCompleteHandle;
	FDelegateHandle LogoutCompleteHandle;

	// PUID 폴링: IOnlineIdentity::OnLoginComplete 콜백 이후
	// FUserManagerEOS::UpdateLocalUser가 LocalUserNum=0에 ProductUserId를 매핑할 때까지 대기.
	// 이 매핑이 끝나기 전에 매칭을 진행하면 P2P 소켓이 LocalBindAddr를 얻지 못해
	// "Could not bind local address" 에러가 발생함. (Bug: EOS_P2P_Bind_Failure.md 참조)
	FTSTicker::FDelegateHandle PuidValidationTickerHandle;
	double PuidValidationStartTime = 0.0;

	static constexpr float MaxPuidValidationSeconds = 5.0f;
	static constexpr float PuidValidationPollInterval = 0.1f;

	void StartPuidValidationPolling(int32 LocalUserNum, const FString& CachedErrorStr);
	bool IsLocalPuidValid(int32 LocalUserNum) const;
	bool TickPuidValidation(float DeltaTime, int32 LocalUserNum, FString CachedErrorStr);
};
