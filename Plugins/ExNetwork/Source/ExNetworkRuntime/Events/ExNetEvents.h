// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * ExNetwork 공통 델리게이트 정의 (C++ 전용).
 *
 * BP 구독이 필요한 Dynamic Multicast 델리게이트는
 * UObject 헤더 (ExOnlineSubsystem.h 등) 에 직접 선언한다.
 * UHT 처리 규칙 (.generated.h 순서 제약) 때문이다.
 */

// ------------------------------------------------------------------
// 인증 이벤트 (Phase 2)
// ------------------------------------------------------------------

/** 로그인 완료 (C++ 구독용) */
DECLARE_MULTICAST_DELEGATE_TwoParams(FExOnLoginCompleteDelegate, bool /*bSuccess*/, const FString& /*ErrorMessage*/);

/** 로그아웃 완료 (C++ 구독용) */
DECLARE_MULTICAST_DELEGATE_OneParam(FExOnLogoutCompleteDelegate, bool /*bSuccess*/);

// ------------------------------------------------------------------
// Lobby 이벤트 (Phase 3)
// ------------------------------------------------------------------

/** Lobby 생성 완료 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FExOnLobbyCreateCompleteDelegate, bool /*bSuccess*/, const FString& /*ErrorMessage*/);

/** Lobby 검색 완료. ResultCount: 찾은 Lobby 수 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FExOnLobbyFindCompleteDelegate, bool /*bSuccess*/, int32 /*ResultCount*/);

/** Lobby 참가 완료 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FExOnLobbyJoinCompleteDelegate, bool /*bSuccess*/, const FString& /*ErrorMessage*/);

/** Lobby 파괴 완료 */
DECLARE_MULTICAST_DELEGATE_OneParam(FExOnLobbyDestroyCompleteDelegate, bool /*bSuccess*/);

// ------------------------------------------------------------------
// Quick Match 이벤트 (Phase 3) — BP용 Dynamic은 ExOnlineSubsystem.h에 선언
// ------------------------------------------------------------------

/** Quick Match 최종 완료 (C++ 구독용) */
DECLARE_MULTICAST_DELEGATE_TwoParams(FExOnMatchFoundDelegate, bool /*bSuccess*/, const FString& /*ErrorMessage*/);
