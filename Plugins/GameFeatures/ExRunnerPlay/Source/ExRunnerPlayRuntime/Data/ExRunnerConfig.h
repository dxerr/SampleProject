// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/Base/ExConfigDataAsset.h"
#include "Struct/FExCurveSettings.h"
#include "Struct/FExObstacleSpawnSettings.h"
#include "ExRunnerConfig.generated.h"

/**
 * UExRunnerConfig
 * 커브 경로, 장애물 생성 등 러너 게임 모드의 핵심 전역 설정(Config) 데이터
 * 통합된 DataCenter 시스템에 등록되어 전역 접근이 가능합니다.
 */
UCLASS(BlueprintType, Blueprintable)
class EXRUNNERPLAYRUNTIME_API UExRunnerConfig : public UExConfigDataAsset
{
	GENERATED_BODY()

public:

	/** 커브 관련 설정 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runner Config|Curve", meta = (ShowOnlyInnerProperties))
	FExCurveSettings Curve;

	/** 장애물 생성 관련 설정 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runner Config|Obstacle", meta = (ShowOnlyInnerProperties))
	FExObstacleSpawnSettings ObstacleSpawn;

#if WITH_EDITOR
	/** 에디터 상에서 데이터의 유효성을 검증합니다. */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
