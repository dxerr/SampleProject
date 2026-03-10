// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameModes/ExGameModeBase.h"
#include "ExLobbyGameMode.generated.h"

/**
 * 로비(프론트엔드) 맵 전용 기본 GameMode입니다.
 * 게임 플레이 로직(청크 생성 등)이 없으며 메뉴, 설정, 세션 매칭 용도로만 사용됩니다.
 */
UCLASS()
class EXCORERUNTIME_API AExLobbyGameMode : public AExGameModeBase
{
	GENERATED_BODY()
	
public:
	AExLobbyGameMode();
};
