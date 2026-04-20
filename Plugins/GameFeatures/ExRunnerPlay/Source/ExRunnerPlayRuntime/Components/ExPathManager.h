/**
 * @file ExPathManager.h
 * @brief 경로 세그먼트 생성 및 관리 컴포넌트
 * @details 러너 게임의 커브 경로를 생성하고, 누적 거리로
 *          월드 위치/방향을 조회하며, 꽈배기 높이 바운딩을 관리
 * 
 * Copyright ExFrameWork. All Rights Reserved.
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../Struct/FExPathSegment.h"
#include "ExPathManager.generated.h"

class UExRunnerConfig;

/**
 * UExPathManager
 * 경로 세그먼트를 동적으로 생성하고 관리하는 컴포넌트
 * GameMode에 부착하여 사용
 */
UCLASS(ClassGroup=(ExCore), meta=(BlueprintSpawnableComponent))
class EXRUNNERPLAYRUNTIME_API UExPathManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UExPathManager();

	/** 커브 설정 데이터 에셋 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Path")
	TWeakObjectPtr<UExRunnerConfig> RunnerConfig;

	// ========== 경로 조회 ==========

	/**
	 * 누적 거리에 해당하는 월드 위치 반환
	 * @param Distance 경로 상 누적 거리 (cm)
	 * @return 월드 위치
	 */
	UFUNCTION(BlueprintPure, Category = "Path")
	FVector GetPositionAtDistance(float Distance) const;

	/**
	 * 누적 거리에 해당하는 진행 방향 반환
	 * @param Distance 경로 상 누적 거리 (cm)
	 * @return 진행 방향 (접선)
	 */
	UFUNCTION(BlueprintPure, Category = "Path")
	FRotator GetDirectionAtDistance(float Distance) const;

	/**
	 * 특정 거리가 커브 구간인지 판정 (장애물 배치용)
	 * @param Distance 경로 상 누적 거리
	 * @return 커브 구간이면 true
	 */
	UFUNCTION(BlueprintPure, Category = "Path")
	bool IsInCurveSection(float Distance) const;

	/**
	 * 월드 위치에 가장 가까운 경로 상의 거리 반환 (투영)
	 * @param WorldLocation 조회할 월드 위치
	 * @param ApproximateDistance 대략적인 거리 (검색 범위 최적화용)
	 * @param SearchRadius 검색 범위 (cm)
	 * @return 가장 가까운 경로 상의 거리
	 */
	UFUNCTION(BlueprintPure, Category = "Path")
	float GetClosestDistanceAtLocation(const FVector& WorldLocation, float ApproximateDistance, float SearchRadius = 2000.f) const;

	/**
	 * 현재 위치의 세그먼트 타입 반환
	 * @param Distance 경로 상 누적 거리
	 */
	UFUNCTION(BlueprintPure, Category = "Path")
	EExPathSegmentType GetSegmentTypeAtDistance(float Distance) const;

	/**
	 * 다음 세그먼트를 생성 (ChunkSpawner가 청크 스폰 시 호출)
	 * @return 생성된 세그먼트 참조
	 */
	UFUNCTION(BlueprintCallable, Category = "Path")
	const FExPathSegment& GenerateNextSegment();

	/**
	 * 경로 초기화 (게임 시작 시)
	 * @param StartPosition 시작 위치
	 * @param StartDirection 시작 방향
	 */
	UFUNCTION(BlueprintCallable, Category = "Path")
	void InitializePath(const FVector& StartPosition, const FRotator& StartDirection);

	/**
	 * 월드 시프트 보정 (ShiftWorld 시 호출)
	 * @param DeltaOffset 시프트 벡터
	 */
	void ShiftPathOrigin(const FVector& DeltaOffset);

	/**
	 * 전체 경로의 현재 총 길이 반환
	 */
	UFUNCTION(BlueprintPure, Category = "Path")
	float GetTotalPathLength() const;

	/**
	 * [User Request Implementation] Circular Arc Math Verification
	 * Calculates Relative Transform based on DistanceTraveled for a 90-degree curve with 1000u arc length.
	 */
	UFUNCTION(BlueprintPure, Category = "Path|Math")
	static FTransform GetPoseOnCurve(float DistanceTraveled);

	/** 세그먼트 목록 접근자 */
	const TArray<FExPathSegment>& GetSegments() const { return PathSegments; }

	/**
	 * 지정 거리까지의 이전 세그먼트 정리
	 * 메모리 최적화를 위해 캐릭터가 지나간 세그먼트 제거
	 * @param MinDistance 이 거리 이전 세그먼트는 제거
	 */
	void CleanupSegmentsBefore(float MinDistance);

	// ── 시각적 힌트 확장점 (추후 구현) ──
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCurveApproaching, 
		float, DistanceToCurve, 
		EExPathSegmentType, CurveDirection);

	UPROPERTY(BlueprintAssignable, Category = "Path|Events")
	FOnCurveApproaching OnCurveApproaching;

protected:
	virtual void BeginPlay() override;

private:
	/** 경로 세그먼트 배열 (순서대로 연결) */
	UPROPERTY()
	TArray<FExPathSegment> PathSegments;

	/** 연속 직선 청크 카운터 (빈도 공식용) */
	int32 ConsecutiveStraightCount = 0;

	// ========== Quadrant Logic State ==========

	/** 마지막 세그먼트의 회전 방향 (직선/좌/우) */
	EExPathSegmentType LastTurnType = EExPathSegmentType::Straight;

	/** 동일 방향 회전 연속 횟수 (꽈배기 감지용) */
	int32 ConsecutiveTurnCount = 0;

	/** 현재 적용 중인 Pitch (경사) 각도 */
	float CurrentPitch = 0.f;

	// ── 치트 상태 관리용 변수 ──
	float CheatCurrentHeight = 0.f;
	bool bCheatAscending = true;
	EExPathSegmentType CheatCurrentCurve = EExPathSegmentType::CurveLeft;

	// ── 내부 함수 ──

	/** 커브/직선 결정 및 세그먼트 데이터 생성 */
	FExPathSegment CreateSegment();

	/**
	 * 꽈배기 높이 계산 (360° 회전 감지 시)
	 * @param NewAngle 새 커브의 각도
	 * @param NewDirection 새 커브의 방향
	 * @return 적용할 높이 오프셋
	 */
	float CalculateHeightForCurve(float NewAngle, EExPathSegmentType NewDirection);

	/** 누적 거리로 세그먼트 인덱스 조회 */
	int32 FindSegmentIndexAtDistance(float Distance) const;
};
