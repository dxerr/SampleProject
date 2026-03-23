// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FExItemSpawnEntry.generated.h"

class UExItemDefinition;

/**
 * 스폰 테이블의 단일 엔트리.
 * 아이템 정의, 선택 가중치, 등장 조건을 담는다.
 */
USTRUCT(BlueprintType)
struct EXCORERUNTIME_API FExItemSpawnEntry
{
	GENERATED_BODY()

	/** 스폰할 아이템 정의 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	TObjectPtr<UExItemDefinition> ItemDefinition;

	/** 선택 가중치 (높을수록 자주 등장) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn", meta = (ClampMin = "0"))
	float Weight = 1.0f;

	/** 이 아이템이 등장하기 위한 최소 속도 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	float MinSpeedRequired = 0.f;
};
