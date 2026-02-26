// Copyright ExFrameWork. All Rights Reserved.
// Gap(구멍/점프) 장애물 스폰 전략

#pragma once

#include "CoreMinimal.h"
#include "ExObstacleSpawnStrategy.h"
#include "ExObstacleStrategy_Gap.generated.h"

/**
 * UExObstacleStrategy_Gap
 * 바닥에 구멍을 만들어 점프로 건너뛰는 장애물 전략
 *
 * - ChunkFloor에 Gap을 적용하여 바닥 메시를 부분 제거
 * - 캐릭터는 점프로 구멍을 건너뛰어야 함
 * - X축: Gap 폭 (MinSize.X ~ MaxSize.X 랜덤)
 * - Gap 장애물 액터 자체는 보이지 않는 트리거
 *
 * 호출 순서:
 * 1. CalculateSpawnPosition → CachedSpawnX에 X 좌표 저장
 * 2. ConfigureObstacle → CachedSpawnX 사용하여 Gap 로컬 좌표 계산
 * 3. Manager에서 위치 적용
 */
UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced, DisplayName = "Gap 전략")
class EXRUNNERPLAYRUNTIME_API UExObstacleStrategy_Gap : public UExObstacleSpawnStrategy
{
	GENERATED_BODY()

public:
	// 장애물 스케일 설정 + Floor Gap 적용
	virtual void ConfigureObstacle_Implementation(AActor* Obstacle,
	                                              const UExObstacleDefinition* Def,
	                                              AExFloorChunk* Chunk) override;

	// 스폰 위치 계산 (바닥 레벨)
	virtual FTransform CalculateSpawnPosition_Implementation(const UExObstacleDefinition* Def,
	                                                      AExFloorChunk* Chunk,
	                                                      float SafeStartX) override;

private:
	// CalculateSpawnPosition에서 계산된 곡선상의 거리(LocalDist) 캐시
	// ConfigureObstacle에서 사용 (호출 순서 보장)
	float CachedSpawnDist = 0.f;
};
