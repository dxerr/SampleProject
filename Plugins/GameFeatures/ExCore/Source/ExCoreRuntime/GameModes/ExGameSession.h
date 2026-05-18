// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameSession.h"
#include "ExGameSession.generated.h"

/**
 * ExCore의 커스텀 게임 세션 클래스입니다.
 * 맵 전환 시 불필요하고 파괴적인 자동 로그인(AutoLogin)을 방지하여 기존 로그인 세션을 완벽하게 안전 구역에 보존합니다, 주인님.
 */
UCLASS()
class EXCORERUNTIME_API AExGameSession : public AGameSession
{
	GENERATED_BODY()

public:
	/** 
	 * 자동 로그인 프로세스를 재정의하여 우회시킵니다.
	 * @return 항상 true를 반환하여 비동기 호출 없이 성공 처리로 유도합니다.
	 */
	virtual bool ProcessAutoLogin() override;
};
