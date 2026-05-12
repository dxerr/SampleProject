// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/IExAuthProvider.h"

#if WITH_EOS_SDK
#include "eos_connect_types.h"
#endif

class IOnlineSubsystemEOS;

/**
 * FExEOSAuthProvider
 *
 * IExAuthProvider의 EOS 구현체.
 * EOS Connect Device ID 익명 로그인을 EOS SDK 직접 호출로 수행한다.
 *
 * 공식 OnlineSubsystemEOS의 IOnlineIdentity::Login()은 EAS(Epic Account) 기반
 * 인증만 지원하고 Device ID를 지원하지 않으므로, EOS SDK API를 직접 호출한다.
 *
 * 로그인 흐름:
 *   1. IOnlineSubsystemEOS::GetEOSPlatformHandle() → EOS_HPlatform 획득
 *   2. EOS_Connect_CreateDeviceId() → 디바이스 고유 ID 생성 (최초 1회, 이미 있으면 스킵)
 *   3. EOS_Connect_Login() with EOS_ECT_DEVICEID_ACCESS_TOKEN → 익명 로그인
 */
class FExEOSAuthProvider : public IExAuthProvider
{
public:

	explicit FExEOSAuthProvider();
	virtual ~FExEOSAuthProvider() override = default;

	virtual void Login(int32 LocalUserNum) override;
	virtual void Logout(int32 LocalUserNum) override;
	virtual bool IsLoggedIn(int32 LocalUserNum) const override;

private:

#if WITH_EOS_SDK
	void OnCreateDeviceIdComplete(const EOS_Connect_CreateDeviceIdCallbackInfo* Data, int32 LocalUserNum);
	void OnConnectLoginComplete(const EOS_Connect_LoginCallbackInfo* Data, int32 LocalUserNum);
#endif

	bool bLoggedIn = false;
};
