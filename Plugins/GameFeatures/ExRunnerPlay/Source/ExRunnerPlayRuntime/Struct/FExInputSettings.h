// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputStrategies/ExRunnerInputMode.h"
#include "FExInputSettings.generated.h"

USTRUCT(BlueprintType)
struct EXRUNNERPLAYRUNTIME_API FExInputSettings
{
	GENERATED_BODY()

public:
	// 조이스틱 값을 실제 회전 델타로 적용할 민감도 스케일
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ExRunner|Input")
	float RunnerLookSensitivity = 0.5f;

	/** 기본 입력 모드 (Manual ↔ AutoRun 등) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ExRunner|Input")
	EExRunnerInputMode DefaultInputMode = EExRunnerInputMode::Manual;
};
