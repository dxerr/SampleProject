// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FExCurveSettings.generated.h"

/**
 * FExCurveSettings
 * 커브 경로 형성 및 빈도를 제어하기 위한 설정 데이터
 */
USTRUCT(BlueprintType)
struct EXRUNNERPLAYRUNTIME_API FExCurveSettings
{
	GENERATED_BODY()

	// ========== 고정 커브 설정 (Quadrant System) ==========

	/** 고정 커브 반경 (cm) - 90도 회전 시 사용 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curve|Quadrant", meta = (ClampMin = "500.0"))
	float FixedCurveRadius = 1500.f;

	/** 
	 * 꽈배기(Spiral) 방지용 경사 각도 (도)
	 * 동일 방향 회전이 누적될 때 적용할 Pitch 각도
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curve|Quadrant", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float SlopePitchAngle = 5.f;

	/** 
	 * 경사를 적용할 연속 회전 횟수
	 * 예: 2회 이상 연속 좌회전 시 SlopePitchAngle 적용
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curve|Quadrant", meta = (ClampMin = "2"))
	int32 SlopeTriggerCount = 2;

	// ========== 커브 빈도 패턴 ==========

	/** 커브 사이 최소 직선 청크 수 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curve|Frequency", meta = (ClampMin = "1"))
	int32 MinStraightChunks = 2;

	/** 커브 사이 최대 직선 청크 수 (이 값에 도달하면 100% 커브 발생) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curve|Frequency", meta = (ClampMin = "2"))
	int32 MaxStraightChunks = 5;

	/**
	 * 기본 커브 발생 확률 (MinStraightChunks 이후 적용)
	 * 공식: P = min(1.0, Base + (연속직선수 - MinStraight) * Growth)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curve|Frequency", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CurveProbabilityBase = 0.4f;

	/** 직선 1개 추가마다 커브 확률 증가량 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curve|Frequency", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CurveProbabilityGrowth = 0.2f;

	// ========== Bounding Box (월드 한계) ==========

	/** 월드 이탈 방지를 위한 X축 한계점 (Min, Max) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curve|Bounds")
	FVector2D WorldBoundsX = FVector2D(-50000.f, 50000.f);

	/** 월드 이탈 방지를 위한 Y축 한계점 (Min, Max) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curve|Bounds")
	FVector2D WorldBoundsY = FVector2D(-50000.f, 50000.f);

	// ========== Spline Mesh ==========

	/** 커브 1개당 Spline Mesh 분할 수 (많을수록 부드럽지만 비용 증가) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curve|Mesh", meta = (ClampMin = "3", ClampMax = "20"))
	int32 SplineSegmentCount = 10;

	// ========== 캐릭터 회전 ==========

	/** 커브 진입 시 캐릭터 회전 보간 속도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curve|Character", meta = (ClampMin = "1.0"))
	float CharacterRotationInterpSpeed = 5.f;

	// ========== 헬퍼 함수 ==========

	/**
	 * 연속 직선 수에 따른 커브 발생 확률 계산
	 * @param ConsecutiveStraightCount 연속 직선 청크 수
	 * @return 커브 발생 확률 (0~1)
	 */
	float GetCurveProbability(int32 ConsecutiveStraightCount) const
	{
		if (ConsecutiveStraightCount < MinStraightChunks)
		{
			return 0.f;
		}
		if (ConsecutiveStraightCount >= MaxStraightChunks)
		{
			return 1.f;
		}
		return FMath::Min(1.f, CurveProbabilityBase + 
			(ConsecutiveStraightCount - MinStraightChunks) * CurveProbabilityGrowth);
	}
};
