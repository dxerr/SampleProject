// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExBeatSyncSettings.generated.h"

/**
 * BeatSync 컴포넌트 동작을 제어하는 설정 구조체.
 * UExBeatSyncComponent::InitSettings()를 통해 외부에서 주입합니다.
 */
USTRUCT(BlueprintType)
struct EXCORERUNTIME_API FExBeatSyncSettings
{
	GENERATED_BODY()

public:
	/** 비트 동기화 활성화 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BeatSync")
	bool bBeatSyncEnabled = true;

	/** 각 비트 틱마다 OnBeatTick이 발동될 확률 (0.0 ~ 1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BeatSync", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpawnProbabilityPerBeat = 0.5f;

	/** 강박(Strong Beat)일 때 보너스 확률 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BeatSync", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StrongBeatBonus = 0.2f;
};
