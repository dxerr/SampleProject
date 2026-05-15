// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Match/ExMatchTypes.h"
#include "Events/ExNetEvents.h"

/**
 * IExLobbyProvider
 *
 * Lobby 백엔드(EOS Lobby 등)와 무관하게 동일한 인터페이스로 Lobby를 처리하는 추상 인터페이스.
 */
class IExLobbyProvider
{
public:

	virtual ~IExLobbyProvider() = default;

	virtual void CreateLobby(const FExMatchConfig& Config) = 0;
	virtual void FindLobbies(const FExMatchConfig& Config) = 0;
	virtual void JoinLobby(int32 ResultIndex) = 0;
	virtual void DestroyLobby() = 0;
	virtual bool IsInLobby() const = 0;
	virtual bool HasLocalSession() const = 0;

	/** 현재 Lobby의 참가 인원 수 반환 */
	virtual int32 GetCurrentPlayerCount() const = 0;

	/** JoinLobby 성공 후 호스트 서버에 접속하기 위한 ConnectString 반환 */
	virtual FString GetConnectString() const = 0;

	/** Lobby 생성 완료 */
	FExOnLobbyCreateCompleteDelegate OnCreateComplete;

	/** Lobby 검색 완료 */
	FExOnLobbyFindCompleteDelegate OnFindComplete;

	/** Lobby 참가 완료 */
	FExOnLobbyJoinCompleteDelegate OnJoinComplete;

	/** Lobby 파괴 완료 */
	FExOnLobbyDestroyCompleteDelegate OnDestroyComplete;

	/**
	 * 상대방이 Lobby에 참가하여 정원이 채워졌을 때 브로드캐스트.
	 * 호스트(Lobby 생성자)가 게임 시작 타이밍을 결정하기 위해 사용.
	 * int32: 현재 참가 인원 수
	 */
	FExOnLobbyParticipantsFullDelegate OnParticipantsFull;
};
