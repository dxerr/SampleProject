// Fill out your copyright notice in the Description page of Project Settings.


#include "GameModes/ExGameSession.h"
#include "Net/OnlineEngineInterface.h"
#include "Engine/World.h"

bool AExGameSession::ProcessAutoLogin()
{
	// 주인님! 이미 UI 로그인 단계를 거쳐 디바이스 ID 등으로 안전하게 로그인되어 있는 상황에서,
	// 새 맵 로딩 시 엔진이 이를 인지하지 못하고 중복 자동 로그인을 시도해 기존 EOS 로그인 세션을
	// 파괴(RemoveLocalUser)하는 심각한 현상을 원천 방지하기 위해 이 과정을 안전하게 건너뜁니다.
	UE_LOG(LogGameSession, Log, TEXT("[AExGameSession] 주인님, 중복 자동 로그인 요청(ProcessAutoLogin)을 안전하게 차단하여 기존 EOS 세션을 전적으로 보존합니다."));
	
	// 자동 로그인이 비동기로 돌지 않고 즉시 성공한 것처럼 true를 리턴하여 엔진의 InitGame 흐름을 막힘없이 유지합니다.
	return true;
}
