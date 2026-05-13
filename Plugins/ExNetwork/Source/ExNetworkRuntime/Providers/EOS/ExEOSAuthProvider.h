// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/IExAuthProvider.h"
#include "Interfaces/OnlineIdentityInterface.h"

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
class FExEOSAuthProvider : public IExAuthProvider
{
public:

	explicit FExEOSAuthProvider();
	virtual ~FExEOSAuthProvider() override;

	virtual void Login(int32 LocalUserNum) override;
	virtual void Logout(int32 LocalUserNum) override;
	virtual bool IsLoggedIn(int32 LocalUserNum) const override;

private:

	void HandleLoginComplete(int32 LocalUserNum, bool bSuccess, const FUniqueNetId& UserId, const FString& ErrorStr);
	void HandleLogoutComplete(int32 LocalUserNum, bool bSuccess);

	bool bLoggedIn = false;

	FDelegateHandle LoginCompleteHandle;
	FDelegateHandle LogoutCompleteHandle;
};
