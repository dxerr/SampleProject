// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/Base/ExConfigDataAsset.h"
#include "Struct/FExCurveSettings.h"
#include "Struct/FExObstacleSpawnSettings.h"
#include "Struct/FExInputSettings.h"
#include "Struct/FExGameplaySettings.h"
#include "Struct/FExMovementSettings.h"
#include "Struct/FExChunkSpawnSettings.h"
#include "Struct/FExBeatSyncSettings.h"
#include "Struct/FExMatchFlowSettings.h"
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

	/** 입력(사용자 제어) 관련 설정 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runner Config|Input", meta = (ShowOnlyInnerProperties))
	FExInputSettings Input;

	/** 조작 민감도 등 기타 게임플레이 관련 설정 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runner Config|Gameplay", meta = (ShowOnlyInnerProperties))
	FExGameplaySettings Gameplay;

	/** 이동 및 레인 전환 설정 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runner Config|Movement", meta = (ShowOnlyInnerProperties))
	FExMovementSettings Movement;

	/** 청크 스팬 시스템 설정 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runner Config|ChunkSpawn", meta = (ShowOnlyInnerProperties))
	FExChunkSpawnSettings ChunkSpawn;

	/** 비트 동기화(리듬) 관련 설정 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runner Config|BeatSync", meta = (ShowOnlyInnerProperties))
	FExBeatSyncSettings BeatSync;

	/** 매치 동기화 및 플로우 관리 설정 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runner Config|MatchFlow", meta = (ShowOnlyInnerProperties))
	FExMatchFlowSettings MatchFlow;

	/**
	 * 로비 입장 시 백그라운드 프리로드(캐시 워밍) 대상 인게임 에셋 목록.
	 * UExAssetPreloadSubsystem이 로비 진입 즉시 LoadPriority=0 (최하위)으로 비동기 로드한다.
	 * 맵 트래블 후 GC에 의해 캐시가 소실될 수 있으므로, 인게임에서 사용하는 시스템이
	 * 직접 강참조를 확보해야 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runner Config|Preload",
		meta = (DisplayName = "인게임 프리로드 에셋 목록"))
	TArray<TSoftObjectPtr<UObject>> IngamePreloadAssets;

#if WITH_EDITOR
	/** 에디터 상에서 데이터의 유효성을 검증합니다. */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
