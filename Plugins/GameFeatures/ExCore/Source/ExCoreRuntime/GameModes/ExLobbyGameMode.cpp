// Fill out your copyright notice in the Description page of Project Settings.

#include "GameModes/ExLobbyGameMode.h"

AExLobbyGameMode::AExLobbyGameMode()
{
	// 로비 모드에서는 틱 업데이트 시 무거운 연산이 없도록 제한하거나 초기 설정 수행 가능
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 로비에서는 기본적으로 Seamless Travel이 필요 없을 수도 있지만,
	// 호스트가 데디/리슨 서버로 맵을 넘어갈 때 클라이언트들이 안 끊기게 하려면 true로 유지
	bUseSeamlessTravel = true;
}
