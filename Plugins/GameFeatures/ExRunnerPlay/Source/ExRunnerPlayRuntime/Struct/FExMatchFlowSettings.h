#pragma once

#include "CoreMinimal.h"
#include "FExMatchFlowSettings.generated.h"

USTRUCT(BlueprintType)
struct EXRUNNERPLAYRUNTIME_API FExMatchFlowSettings
{
	GENERATED_BODY()

public:
	/** 
	 * 매치에 필요한 전체 플레이어 수 (이 인원수가 다 모이면 카운트다운 시작)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MatchFlow")
	int32 ExpectedPlayerCount = 1;

	/**
	 * 모두 모인 뒤 게임 시작까지 카운트다운(초)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MatchFlow")
	int32 CountdownDurationSeconds = 3;

	/**
	 * 플레이어 연결 대기 타임아웃(초). 이 시간 안에 다 안 모이면 강제 시작 처리.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MatchFlow")
	float MaxWaitForPlayersSeconds = 30.f;

	/**
	 * 멀티플레이 시 레인 할당 순서. (예: 0, -1, 1)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MatchFlow")
	TArray<int32> LaneSlotOrder = {0, -1, 1};
};
