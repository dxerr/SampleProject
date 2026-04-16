// Fill out your copyright notice in the Description page of Project Settings.

#include "GameModes/ExLobbyGameMode.h"
#include "Subsystems/ExGameFlowSubsystem.h"
#include "Tags/ExFlowTags.h"

AExLobbyGameMode::AExLobbyGameMode()
{
	// 로비 모드에서는 틱 업데이트 시 무거운 연산이 없도록 제한하거나 초기 설정 수행 가능
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 로비에서는 기본적으로 Seamless Travel이 필요 없을 수도 있지만,
	// 호스트가 데디/리슨 서버로 맵을 넘어갈 때 클라이언트들이 안 끊기게 하려면 true로 유지
	bUseSeamlessTravel = true;
}

void AExLobbyGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 로비 맵 진입 시 GameFlow 상태를 Flow.Lobby로 전환합니다.
	// 정식 빌드에서는 Auth_IDP를 거쳐야 하지만, 인증 우회 환경에서는 Boot → Lobby 직접 전환을 허용합니다.
	// (ExGameFlowSubsystem AllowedTransitions에 Boot → Lobby 전환이 등록되어 있어야 합니다.)
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UExGameFlowSubsystem* FlowSys = GI->GetSubsystem<UExGameFlowSubsystem>())
		{
			FlowSys->SetFlowState(ExFlowTags::Flow_Lobby);
		}
	}
}
