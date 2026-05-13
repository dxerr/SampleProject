// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ExNetworkTypes.h"
#include "ExMatchTypes.generated.h"

/** 매칭 세션 이름 상수. UE 세션 API 호출 시 일관된 이름 사용. */
static const FName ExMatchSessionName = TEXT("ExMatch");

/**
 * 매칭 상태 머신.
 * UExOnlineSubsystem::GetMatchState() 으로 외부에서 조회 가능.
 */
UENUM(BlueprintType)
enum class EExMatchState : uint8
{
	Idle        UMETA(DisplayName = "Idle"),
	Searching   UMETA(DisplayName = "Searching"),
	Creating    UMETA(DisplayName = "Creating"),
	Waiting     UMETA(DisplayName = "Waiting"),
	Joining     UMETA(DisplayName = "Joining"),
	Ready       UMETA(DisplayName = "Ready"),
};

/**
 * FExMatchConfig
 *
 * Quick Match 요청 시 전달하는 매칭 설정 구조체.
 */
USTRUCT(BlueprintType)
struct FExMatchConfig
{
	GENERATED_BODY()

	/** 최대 플레이어 수 (기본 2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	int32 MaxPlayers = 2;

	/**
	 * 게임 모드 식별자. Lobby 검색 필터로 사용된다.
	 * 예: "Runner", "Battle"
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	FString MatchMode = TEXT("Default");

	/**
	 * 게임 시작 시 전환할 맵 경로. Phase 4(ServerTravel)에서 활용.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	FString MapPath = TEXT("");
};
