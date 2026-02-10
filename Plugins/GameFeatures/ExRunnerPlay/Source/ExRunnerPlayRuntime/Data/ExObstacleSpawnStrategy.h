// Copyright ExFrameWork. All Rights Reserved.
// 장애물 스폰 전략 베이스 클래스
// EExObstacleType별로 서브클래스를 구현하여 타입별 배치/크기/위치 로직을 캡슐화합니다.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ExObstacleSpawnStrategy.generated.h"

class AActor;
class AExFloorChunk;
class UExObstacleDefinition;

/**
 * UExObstacleSpawnStrategy
 * 장애물 타입별 스폰 전략의 추상 베이스 클래스 (Strategy Pattern)
 *
 * - Blueprintable: BP 서브클래스 생성 가능 (간단한 노드 작성으로 새 타입 대응)
 * - EditInlineNew + DefaultToInstanced: 에디터 Details 패널에서 인라인 편집
 * - BlueprintNativeEvent: C++ 기본 구현 + BP 오버라이드 모두 지원
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class EXRUNNERPLAYRUNTIME_API UExObstacleSpawnStrategy : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 장애물 액터의 스케일/크기/모양을 설정합니다.
	 * 타입별로 완전히 다른 설정이 가능합니다.
	 *
	 * @param Obstacle   풀에서 가져오거나 새로 생성된 장애물 액터
	 * @param Def        장애물 메타데이터 (크기 범위, 타입별 파라미터 등)
	 * @param Chunk      장애물이 배치될 바닥 청크
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Obstacle|Strategy")
	void ConfigureObstacle(AActor* Obstacle, const UExObstacleDefinition* Def,
	                       AExFloorChunk* Chunk);
	virtual void ConfigureObstacle_Implementation(AActor* Obstacle,
	                                              const UExObstacleDefinition* Def,
	                                              AExFloorChunk* Chunk);

	/**
	 * 장애물의 월드 스폰 위치를 계산합니다.
	 * 타입별로 Z오프셋, Y오프셋 등이 다를 수 있습니다.
	 *
	 * @param Def          장애물 메타데이터
	 * @param Chunk        배치 대상 청크
	 * @param SafeStartX   배치 가능한 월드 X 시작 좌표
	 * @return             최종 월드 스폰 위치
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Obstacle|Strategy")
	FVector CalculateSpawnPosition(const UExObstacleDefinition* Def,
	                               AExFloorChunk* Chunk,
	                               float SafeStartX);
	virtual FVector CalculateSpawnPosition_Implementation(const UExObstacleDefinition* Def,
	                                                      AExFloorChunk* Chunk,
	                                                      float SafeStartX);

	/**
	 * 장애물 통과 후 복귀 거리를 계산합니다.
	 * 기본 구현: Def->RecoveryTime * RunSpeed
	 *
	 * @param Def        장애물 메타데이터
	 * @param RunSpeed   현재 러닝 속도
	 * @return           복귀에 필요한 X 거리
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Obstacle|Strategy")
	float GetRecoveryDistance(const UExObstacleDefinition* Def, float RunSpeed);
	virtual float GetRecoveryDistance_Implementation(const UExObstacleDefinition* Def,
	                                                 float RunSpeed);
};
