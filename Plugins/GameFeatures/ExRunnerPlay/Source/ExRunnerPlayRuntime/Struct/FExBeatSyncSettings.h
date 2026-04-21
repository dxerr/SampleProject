// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FExBeatSyncSettings.generated.h"

USTRUCT(BlueprintType)
struct EXRUNNERPLAYRUNTIME_API FExBeatSyncSettings
{
	GENERATED_BODY()

public:
	// 동기화 모드 켜고 끄기
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ExRunner|BeatSync")
	bool bBeatSyncEnabled = true;

	// 각 비트 틱마다 장애물이 배치될 확률 (0.0 ~ 1.0)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ExRunner|BeatSync", meta=(ClampMin="0.0", ClampMax="1.0"))
	float SpawnProbabilityPerBeat = 0.5f;

	// 강박(Strong Beat)일 때 보너스 확률
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ExRunner|BeatSync", meta=(ClampMin="0.0", ClampMax="1.0"))
	float StrongBeatBonus = 0.2f;
};
