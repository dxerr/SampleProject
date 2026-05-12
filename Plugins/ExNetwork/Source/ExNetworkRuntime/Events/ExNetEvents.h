// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * ExNetwork 공통 델리게이트 정의 (C++ 전용).
 *
 * BP 구독이 필요한 Dynamic Multicast 델리게이트는
 * UObject 헤더 (ExOnlineSubsystem.h 등) 에 직접 선언한다.
 * UHT 처리 규칙 (.generated.h 순서 제약) 때문이다.
 *
 * Phase 2: 인증 관련 델리게이트
 * Phase 3 이후: 매칭 관련 델리게이트 추가 예정
 */

// ------------------------------------------------------------------
// 인증 이벤트 (Phase 2)
// ------------------------------------------------------------------

/**
 * 로그인 완료 시 브로드캐스트 (C++ 구독용).
 * IExAuthProvider 구현체가 이 델리게이트를 소유하고 브로드캐스트한다.
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FExOnLoginCompleteDelegate, bool /*bSuccess*/, const FString& /*ErrorMessage*/);

/**
 * 로그아웃 완료 시 브로드캐스트 (C++ 구독용).
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FExOnLogoutCompleteDelegate, bool /*bSuccess*/);

// ------------------------------------------------------------------
// 매칭 이벤트 (Phase 3 이후 추가 예정)
// ------------------------------------------------------------------

// DECLARE_MULTICAST_DELEGATE_OneParam(FExOnMatchFoundDelegate, const FString& /*SessionId*/);
// DECLARE_MULTICAST_DELEGATE_OneParam(FExOnMatchJoinedDelegate, bool /*bSuccess*/);
