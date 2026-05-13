// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Match/ExMatchTypes.h"
#include "Events/ExNetEvents.h"

/**
 * IExLobbyProvider
 *
 * Lobby 백엔드(EOS Lobby 등)와 무관하게 동일한 인터페이스로 Lobby를 처리하는 추상 인터페이스.
 * ExListenServerStrategy가 이 인터페이스를 통해 Lobby 로직을 호출한다.
 *
 * 구현체:
 *   - ExEOSLobbyProvider: EOS IOnlineSession 기반 (Providers/EOS/)
 *
 * 생명주기:
 *   ExListenServerStrategy 생성 시 ExEOSLobbyProvider 주입
 *   ExListenServerStrategy 소멸 시 함께 소멸
 */
class IExLobbyProvider
{
public:

	virtual ~IExLobbyProvider() = default;

	/**
	 * 새 Lobby를 생성한다. (호스트 입장)
	 * 완료 시 OnCreateComplete 델리게이트 브로드캐스트.
	 * @param Config 매칭 설정 (MaxPlayers, MatchMode 등)
	 */
	virtual void CreateLobby(const FExMatchConfig& Config) = 0;

	/**
	 * 조건에 맞는 Lobby를 검색한다.
	 * 완료 시 OnFindComplete 델리게이트 브로드캐스트.
	 * @param Config 검색 필터로 사용할 매칭 설정
	 */
	virtual void FindLobbies(const FExMatchConfig& Config) = 0;

	/**
	 * 검색된 Lobby에 참가한다.
	 * 완료 시 OnJoinComplete 델리게이트 브로드캐스트.
	 * @param ResultIndex FindLobbies 결과의 인덱스
	 */
	virtual void JoinLobby(int32 ResultIndex) = 0;

	/**
	 * 현재 참가 중인 Lobby를 파괴하고 리소스를 정리한다.
	 * 완료 시 OnDestroyComplete 델리게이트 브로드캐스트.
	 */
	virtual void DestroyLobby() = 0;

	/** 현재 Lobby에 참가 중인지 여부 */
	virtual bool IsInLobby() const = 0;

	/** Lobby 생성 완료 (bool: 성공 여부, FString: 에러) */
	FExOnLobbyCreateCompleteDelegate OnCreateComplete;

	/** Lobby 검색 완료 (bool: 성공 여부, int32: 검색 결과 수) */
	FExOnLobbyFindCompleteDelegate OnFindComplete;

	/** Lobby 참가 완료 (bool: 성공 여부, FString: 에러) */
	FExOnLobbyJoinCompleteDelegate OnJoinComplete;

	/** Lobby 파괴 완료 (bool: 성공 여부) */
	FExOnLobbyDestroyCompleteDelegate OnDestroyComplete;
};
