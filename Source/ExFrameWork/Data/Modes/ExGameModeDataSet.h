// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ExGameModeDataSet.generated.h"

/**
 * ExGameModeDataSet
 * 게임 모드별 설정 데이터셋 (예: 점수, 라운드, 규칙 등)
 */
UCLASS(BlueprintType)
class EXFRAMEWORK_API UExGameModeDataSet : public UDataAsset
{
	GENERATED_BODY()

public:
	// 예시 데이터: 최대 점수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GameMode")
	int32 MaxScore = 100;

	// 예시 데이터: 라운드 수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GameMode")
	int32 NumRounds = 3;

	// 1. 컨테이너 폰 클래스 (예: SandboxCharacter_CMC - 이동/물리 담당)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameMode")
	TSubclassOf<APawn> ContainerPawnClass;

	// 2. 로컬 플레이어 캐릭터 클래스 (예: BP_Twinblast - 비주얼/특수 로직 담당)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameMode")
	TSubclassOf<AActor> LocalPlayerClass;
};
