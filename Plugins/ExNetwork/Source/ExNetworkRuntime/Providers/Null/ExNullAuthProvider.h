// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/IExAuthProvider.h"

/**
 * FExNullAuthProvider
 *
 * IExAuthProvider의 Null 구현체. 오프라인/PIE 테스트용.
 * Login() 호출 시 즉시(다음 틱) 로그인 성공을 시뮬레이션한다.
 *
 * 사용 시점:
 *   - EOS OSS를 찾을 수 없는 경우 UExOnlineSubsystem이 자동으로 Fallback
 *   - EOS 서버 연결 없이 매칭 흐름을 테스트할 때
 */
class FExNullAuthProvider : public IExAuthProvider
{
public:

	explicit FExNullAuthProvider();
	virtual ~FExNullAuthProvider() override = default;

	/** IExAuthProvider 구현 — 즉시 성공 시뮬레이션 */
	virtual void Login(int32 LocalUserNum) override;
	virtual void Logout(int32 LocalUserNum) override;
	virtual bool IsLoggedIn(int32 LocalUserNum) const override;

private:

	bool bLoggedIn = false;
};
