// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** 서버 모델 타입 */
UENUM()
enum class EExServerType : uint8
{
	ListenServer,
	DedicatedServer,
};

/**
 * IExNetServerStrategy
 *
 * Listen Server / Dedicated Server 두 모델을 동일한 인터페이스로 추상화한다.
 * UExOnlineSubsystem이 NetMode를 감지하여 적절한 구현체를 자동 선택한다.
 *
 * 구현체:
 *   - ExListenServerStrategy    : 호스트 PC가 서버 역할 (Strategies/)
 *   - ExDedicatedServerStrategy : 별도 서버 인스턴스 사용 (Strategies/, Phase 4+)
 *
 * 게임 코드는 이 인터페이스만 참조하므로 서버 환경 변경 시 코드 수정이 불필요하다.
 */
class IExNetServerStrategy
{
public:

	virtual ~IExNetServerStrategy() = default;

	/**
	 * 현재 전략의 서버 타입을 반환한다.
	 */
	virtual EExServerType GetServerType() const = 0;

	/**
	 * 새 매치를 생성한다. (호스트 입장)
	 * Phase 3에서 EOS Lobby 생성 로직으로 채워진다.
	 */
	virtual void CreateMatch() = 0;

	/**
	 * 기존 매치에 참가한다. (클라이언트 입장)
	 * Phase 3에서 EOS Lobby 참가 로직으로 채워진다.
	 * @param SessionId 참가할 세션 ID
	 */
	virtual void JoinMatch(const FString& SessionId) = 0;

	/**
	 * Lobby → 게임 맵으로 전환을 트리거한다. (Phase 4)
	 * @param MapPath 전환할 맵 경로
	 */
	virtual void StartGameSession(const FString& MapPath) = 0;

	/**
	 * 현재 매치를 종료하고 리소스를 정리한다.
	 */
	virtual void DestroyMatch() = 0;
};
