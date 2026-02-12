/**
 * @file FExPathSegment.h
 * @brief 경로 세그먼트 구조체 정의 (직선/커브)
 * @details 러너 게임의 커브 Floor 시스템에서 경로를 구성하는 최소 단위
 * 
 * Copyright ExFrameWork. All Rights Reserved.
 */

#pragma once

#include "CoreMinimal.h"
#include "FExPathSegment.generated.h"

/**
 * 경로 세그먼트 타입
 */
UENUM(BlueprintType)
enum class EExPathSegmentType : uint8
{
	Straight    UMETA(DisplayName = "직선"),
	CurveLeft   UMETA(DisplayName = "좌커브"),
	CurveRight  UMETA(DisplayName = "우커브")
};

/**
 * FExPathSegment
 * 경로를 구성하는 최소 단위 (직선 또는 커브)
 * PathManager가 생성하고, ChunkSpawner/FloorChunk가 참조
 */
USTRUCT(BlueprintType)
struct EXRUNNERPLAYRUNTIME_API FExPathSegment
{
	GENERATED_BODY()

	/** 세그먼트 타입 (직선/좌커브/우커브) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path")
	EExPathSegmentType Type = EExPathSegmentType::Straight;

	/** 호(Arc) 길이 = 이동 거리 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path")
	float ArcLength = 1000.f;

	/** 커브 총 각도 (도, 직선일 때 0) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path|Curve")
	float CurveAngle = 0.f;

	/** 커브 반경 (cm, 직선일 때 무시) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path|Curve")
	float CurveRadius = 2000.f;

	/** 꽈배기 높이 보정값 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path|Height")
	float HeightOffset = 0.f;

	// ── 런타임 계산 값 (PathManager가 설정) ──

	/** 세그먼트 시작 월드 위치 */
	FVector StartWorldPos = FVector::ZeroVector;

	/** 세그먼트 시작 월드 방향 */
	FRotator StartWorldRot = FRotator::ZeroRotator;

	/** 세그먼트 끝 월드 위치 */
	FVector EndWorldPos = FVector::ZeroVector;

	/** 세그먼트 끝 월드 방향 */
	FRotator EndWorldRot = FRotator::ZeroRotator;

	/** 경로 상 누적 시작 거리 */
	float CumulativeStartDistance = 0.f;

	// ── 보간 함수 ──

	/**
	 * 세그먼트 내 진행률(0~1)에 해당하는 월드 위치 반환
	 * - 직선: 선형 보간
	 * - 커브: 원호(Arc) 보간
	 */
	FVector GetPositionAtAlpha(float Alpha) const;

	/**
	 * 세그먼트 내 진행률(0~1)에 해당하는 월드 방향 반환
	 * - 직선: 시작 방향 유지
	 * - 커브: 접선 방향
	 */
	FRotator GetRotationAtAlpha(float Alpha) const;

	/**
	 * 시작/끝 월드 좌표를 Type/Angle/Radius로부터 계산
	 * PathManager가 세그먼트 추가 시 호출
	 */
	void CalculateEndTransform();
};
