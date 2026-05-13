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
	Idle        UMETA(DisplayName = "Idle"),        // 매칭 대기 없음
	Searching   UMETA(DisplayName = "Searching"),   // Lobby 검색 중
	Creating    UMETA(DisplayName = "Creating"),    // Lobby 생성 중
	Waiting     UMETA(DisplayName = "Waiting"),     // Lobby 생성 완료, 상대 대기 중
	Joining     UMETA(DisplayName = "Joining"),     // 기존 Lobby 참가 중
	Ready       UMETA(DisplayName = "Ready"),       // 매칭 완료 (게임 시작 가능)
	InGame      UMETA(DisplayName = "InGame"),      // ServerTravel 완료, 게임 진행 중
};

/**
 * FExMatchConfig
 *
 * Quick Match 요청 및 게임 시작 시 전달하는 매칭 설정 구조체.
 * GameFeature(ExRunnerPlay 등)가 이 구조체를 통해 매칭 파라미터를 주입한다.
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
	 * 같은 MatchMode끼리만 매칭된다.
	 * 예: "Runner", "Battle"
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	FString MatchMode = TEXT("Default");

	/**
	 * 게임 시작 시 ServerTravel 할 맵 경로.
	 * StartGame() 호출 시 이 경로로 이동한다.
	 * 예: "/ExRunnerPlay/Map/L_ExRunnerTest"
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	FString MapPath = TEXT("");
};
