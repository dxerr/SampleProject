// Copyright ExFrameWork. All Rights Reserved.
// 장애물 스폰 전략 베이스 클래스
// EExObstacleType별로 서브클래스를 구현하여 타입별 배치/크기/위치 로직을 캡슐화합니다.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "../Data/ExObstacleDefinition.h"
#include "ExObstacleSpawnStrategy.generated.h"

class AActor;
class AExFloorChunk;
class UExObstacleDefinition;

/**
 * 장애물 정보 구조체
 * BP에서 장애물의 타입과 주요 수치를 전달받기 위해 사용
 */
USTRUCT(BlueprintType)
struct FExObstacleInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Obstacle")
	EExObstacleType Type = EExObstacleType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Obstacle")
	float Value = 0.0f; // Gap/Slide: Width, Climb: Height
};


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
	// 클라이언트 시각 동기화를 위해 최근 ConfigureObstacle 실행 시 생성된 난수/데이터를 임시 저장 (서버 전용 캐시)
	UPROPERTY(Transient)
	float LastGeneratedInfoValue = 0.f;

	UPROPERTY(Transient)
	float LastGeneratedGapLocalStartDist = -1.f;

	UPROPERTY(Transient)
	float LastGeneratedMeshRelativeScaleY = -1.f;

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
	FTransform CalculateSpawnPosition(const UExObstacleDefinition* Def,
	                               AExFloorChunk* Chunk,
	                               float SafeStartX);
	virtual FTransform CalculateSpawnPosition_Implementation(const UExObstacleDefinition* Def,
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

protected:
	/**
	 * 장애물의 시각적 경계(Bounds)를 가져옵니다.
	 * @param Actor 대상 장애물 액터
	 * @param bUseOriginalMeshExtents 풀링 및 회전 버그 방지를 위해 원본 에셋(스태틱 메시)의 로컬 Bounds 사용 여부
	 */
	UFUNCTION(BlueprintCallable, Category = "Obstacle|Strategy")
	static FBoxSphereBounds GetVisualBounds(AActor* Actor, bool bUseOriginalMeshExtents = false);

	/**
	 * 장애물의 목표 크기(TargetSize)를 기반으로 스케일 값을 계산합니다.
	 * 메시의 기본 크기를 측정하고 0으로 나누는 것을 방지합니다.
	 * @param Obstacle 대상 장애물 액터
	 * @param TargetSize 원하는 월드 기준 크기 (X, Y, Z)
	 * @param bUseOriginalMeshExtents 원본 에셋의 로컬 Bounds 사용 여부
	 * @return 계산된 3D 스케일 벡터
	 */
	UFUNCTION(BlueprintCallable, Category = "Obstacle|Strategy")
	static FVector CalculateObstacleScale(AActor* Obstacle, const FVector& TargetSize, bool bUseOriginalMeshExtents = false);

	/**
	 * 장애물 액터에 타입 및 크기 정보를 주입합니다.
	 * 해당 액터가 IExObstacleInterface를 구현하고 있어야 합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Obstacle|Strategy")
	void ApplyObstacleInfo(AActor* Obstacle, const FExObstacleInfo& Info);

	/**
	 * FloorChunk의 실제 바닥 폭(Y축)을 반환합니다.
	 * ★ 로컬 메시 Bounds × 컴포넌트 스케일로 계산하여
	 *   커브 청크의 회전에 의한 월드 AABB 왜곡 문제를 방지합니다.
	 * @param Chunk 바닥 청크 액터
	 * @return 바닥 폭 (cm 단위, 전체 너비)
	 */
	static float GetFloorWidth(const AExFloorChunk* Chunk);

};
