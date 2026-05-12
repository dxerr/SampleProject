// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Events/ExNetEvents.h"

/**
 * IExAuthProvider
 *
 * 인증 백엔드(EOS/Steam/Custom)와 무관하게 동일한 인터페이스로 인증을 처리하는 추상 인터페이스.
 * UExOnlineSubsystem이 이 인터페이스를 통해 인증 로직을 호출한다.
 *
 * 구현체:
 *   - ExEOSAuthProvider  : EOS Connect Device ID 익명 로그인 (Providers/EOS/)
 *   - ExNullAuthProvider : 오프라인/PIE 테스트용 즉시 성공 시뮬레이션 (Providers/Null/)
 *
 * 생명주기:
 *   UExOnlineSubsystem::Initialize → Provider 생성 → Login 호출
 *   UExOnlineSubsystem::Deinitialize → Provider 소멸
 */
class IExAuthProvider
{
public:

	virtual ~IExAuthProvider() = default;

	/**
	 * 비동기 로그인을 시작한다.
	 * 완료 시 OnLoginComplete 델리게이트가 브로드캐스트된다.
	 * @param LocalUserNum 로컬 플레이어 인덱스 (일반적으로 0)
	 */
	virtual void Login(int32 LocalUserNum) = 0;

	/**
	 * 로그아웃을 요청한다.
	 * 완료 시 OnLogoutComplete 델리게이트가 브로드캐스트된다.
	 * @param LocalUserNum 로컬 플레이어 인덱스
	 */
	virtual void Logout(int32 LocalUserNum) = 0;

	/**
	 * 현재 로그인 상태를 반환한다.
	 * @param LocalUserNum 로컬 플레이어 인덱스
	 * @return 로그인 완료 상태이면 true
	 */
	virtual bool IsLoggedIn(int32 LocalUserNum) const = 0;

	/** 로그인 완료 시 브로드캐스트되는 델리게이트 */
	FExOnLoginCompleteDelegate OnLoginComplete;

	/** 로그아웃 완료 시 브로드캐스트되는 델리게이트 */
	FExOnLogoutCompleteDelegate OnLogoutComplete;
};
