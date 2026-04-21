/**
 * @file ExPathManager.cpp
 * @brief 경로 세그먼트 생성 및 관리 구현
 * @details 세그먼트 생성(빈도 공식), 좌표 조회, 꽈배기 높이 바운딩 처리
 * 
 * Copyright ExFrameWork. All Rights Reserved.
 */

#include "ExPathManager.h"
#include "../Data/ExRunnerConfig.h"
#include "../GameStates/ExRunnerGameState.h"
#include "ExDebugStateSubsystem.h"
#include "ExGameplayTags.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogExPathManager, Log, All);

UExPathManager::UExPathManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UExPathManager::BeginPlay()
{
	Super::BeginPlay();
}

// ──────────────────────────────────────────────
// 경로 초기화
// ──────────────────────────────────────────────
void UExPathManager::InitializePath(const FVector& StartPosition, const FRotator& StartDirection)
{
	PathSegments.Empty();
	ConsecutiveStraightCount = 0;
	// 4분면 시스템 상태 초기화
	LastTurnType = EExPathSegmentType::Straight;
	ConsecutiveTurnCount = 0;
	CurrentPitch = 0.f;

	CheatCurrentHeight = 0.f;
	bCheatAscending = true;
	CheatCurrentCurve = EExPathSegmentType::CurveLeft;

	// 시드 초기화 (GameState의 공유 시드 사용, 만약 유효하지 않다면 보조수단)
	if (AExRunnerGameState* GS = GetWorld()->GetGameState<AExRunnerGameState>())
	{
		PathRandomStream.Initialize(GS->SharedTrackSeed);
	}
	else
	{
		PathRandomStream.GenerateNewSeed();
	}

	// 초기 직선 세그먼트 추가 (시작 구간은 항상 직선)
	// ★ 중요: 첫 청크의 Center가 (0,0,0)에 오도록 하려면
	const float FirstSegLength = RunnerConfig.IsValid() ? 1000.f : 1000.f;
	const float HalfLength = FirstSegLength * 0.5f;

	FExPathSegment InitialSegment;
	InitialSegment.Type = EExPathSegmentType::Straight;
	InitialSegment.ArcLength = FirstSegLength; 
	// StartPosition이 (0,0,0)이라면, 세그먼트 시작은 (-500, 0, 0)
	InitialSegment.StartWorldPos = StartPosition - (StartDirection.Vector() * HalfLength);
	InitialSegment.StartWorldRot = StartDirection;
	// 거리도 -500부터 시작 (Center가 0이 되도록)
	InitialSegment.CumulativeStartDistance = -HalfLength;
	InitialSegment.CalculateEndTransform();

	PathSegments.Add(InitialSegment);
	ConsecutiveStraightCount = 1;

	if (RunnerConfig.IsValid())
	{
		UE_LOG(LogExPathManager, Log, TEXT("경로 초기화 완료: Start=%s, Dir=%s (Config: %s)"),
			*StartPosition.ToString(), *StartDirection.ToString(), *RunnerConfig->GetName());
	}
	else
	{
		UE_LOG(LogExPathManager, Error, TEXT("경로 초기화 실패: RunnerConfig가 유효하지 않습니다! (기본값 사용)"));
	}
}

// ──────────────────────────────────────────────
// 다음 세그먼트 생성
// ──────────────────────────────────────────────
const FExPathSegment& UExPathManager::GenerateNextSegment()
{
	FExPathSegment NewSegment = CreateSegment();

	// 이전 세그먼트의 끝 지점에서 이어붙이기
	if (PathSegments.Num() > 0)
	{
		const FExPathSegment& LastSeg = PathSegments.Last();
		NewSegment.StartWorldPos = LastSeg.EndWorldPos;
		NewSegment.StartWorldRot = LastSeg.EndWorldRot;
		NewSegment.CumulativeStartDistance = LastSeg.CumulativeStartDistance + LastSeg.ArcLength;
	}

	// 끝 지점 계산
	NewSegment.CalculateEndTransform();

	PathSegments.Add(NewSegment);

	UE_LOG(LogExPathManager, Verbose, TEXT("세그먼트 추가 [%d]: Type=%d, Angle=%.1f, Length=%.1f, Height=%.1f"),
		PathSegments.Num() - 1,
		(int32)NewSegment.Type,
		NewSegment.CurveAngle,
		NewSegment.ArcLength,
		NewSegment.HeightOffset);

	return PathSegments.Last();
}

// ──────────────────────────────────────────────
// 세그먼트 생성 (커브/직선 결정 + 데이터 설정)
// ──────────────────────────────────────────────
FExPathSegment UExPathManager::CreateSegment()
{
	FExPathSegment Segment;
	
	if (!RunnerConfig.IsValid())
	{
		Segment.Type = EExPathSegmentType::Straight;
		Segment.ArcLength = 1000.f;
		return Segment;
	}

	// --- 디버그 치트 강제 개입 (TAG_Ex_Debug_Slope) ---
	bool bCheatSlopeActive = false;
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UExDebugStateSubsystem* DS = GI->GetSubsystem<UExDebugStateSubsystem>())
			{
				bCheatSlopeActive = DS->IsCheatEnabled(TAG_Ex_Debug_Slope);
			}
		}
	}

	if (bCheatSlopeActive)
	{
		// 1. 치트 활성화 시 무조건 커브 생성 (다른 확률 무시)
		ConsecutiveStraightCount = 0;
		float HeightLimit = 2000.f; // 임의의 상/하 높이 한계치

		// 2. 한계 도달 시 방향 및 상승/하강 반전
		if (bCheatAscending && CheatCurrentHeight >= HeightLimit)
		{
			bCheatAscending = false;
			CheatCurrentCurve = (CheatCurrentCurve == EExPathSegmentType::CurveLeft) ? EExPathSegmentType::CurveRight : EExPathSegmentType::CurveLeft;
			UE_LOG(LogExPathManager, Warning, TEXT("[Cheat/Slope] 상방 한계 도달! 하강 및 방향 반전."));
		}
		else if (!bCheatAscending && CheatCurrentHeight <= -HeightLimit)
		{
			bCheatAscending = true;
			CheatCurrentCurve = (CheatCurrentCurve == EExPathSegmentType::CurveLeft) ? EExPathSegmentType::CurveRight : EExPathSegmentType::CurveLeft;
			UE_LOG(LogExPathManager, Warning, TEXT("[Cheat/Slope] 하방 한계 도달! 상승 및 방향 반전."));
		}

		Segment.Type = CheatCurrentCurve;
		Segment.CurveAngle = 90.f;
		Segment.CurveRadius = RunnerConfig->Curve.FixedCurveRadius;
		Segment.ArcLength = PI * Segment.CurveRadius * 0.5f;

		// 3. 피치 적용 (상승/하강)
		float TargetPitch = RunnerConfig->Curve.SlopePitchAngle;
		if (!bCheatAscending)
		{
			TargetPitch = -TargetPitch;
		}

		// 4. 높이 오프셋 계산 및 누적
		if (!FMath::IsNearlyZero(TargetPitch))
		{
			const float PitchRad = FMath::DegreesToRadians(TargetPitch);
			Segment.HeightOffset = Segment.ArcLength * FMath::Tan(PitchRad);
		}
		else
		{
			Segment.HeightOffset = 0.f;
		}
		
		CheatCurrentHeight += Segment.HeightOffset;

		LastTurnType = Segment.Type;
		ConsecutiveTurnCount++;

		UE_LOG(LogExPathManager, Log, TEXT("[Cheat/Slope] 꽈배기 유지: %s, Pitch: %.1f, 누적높이: %.1f"), 
			(Segment.Type == EExPathSegmentType::CurveLeft) ? TEXT("좌") : TEXT("우"), TargetPitch, CheatCurrentHeight);

		return Segment;
	}

	// 1. 커브/직선 결정
	const float Probability = RunnerConfig->Curve.GetCurveProbability(ConsecutiveStraightCount);
	bool bShouldCurve = PathRandomStream.FRand() < Probability;

	// ── Bounding Box 강제 커브 판정 ──
	bool bForceCurve = false;
	EExPathSegmentType ForcedTurnDir = EExPathSegmentType::CurveLeft;

	if (PathSegments.Num() > 0)
	{
		const FExPathSegment& LastSeg = PathSegments.Last();
		FVector EndPos = LastSeg.EndWorldPos;
		FVector ForwardDir = LastSeg.EndWorldRot.Vector();
		
		// 직선 세그먼트 스폰 시 예측 끝 지점
		FVector ProjectedNextPos = EndPos + (ForwardDir * 1000.f); 

		if (ProjectedNextPos.X < RunnerConfig->Curve.WorldBoundsX.X || ProjectedNextPos.X > RunnerConfig->Curve.WorldBoundsX.Y ||
			ProjectedNextPos.Y < RunnerConfig->Curve.WorldBoundsY.X || ProjectedNextPos.Y > RunnerConfig->Curve.WorldBoundsY.Y)
		{
			bForceCurve = true;
			bShouldCurve = true;

			// 중심(원점 0,0,0) 방향으로 방향을 틀도록 좌/우 결정
			FVector ToOrigin = FVector::ZeroVector - EndPos;
			ToOrigin.Z = 0.f;

			// 현재 진행방향의 오른쪽 벡터
			FVector RightDir = FRotator(0, LastSeg.EndWorldRot.Yaw + 90.f, 0).Vector();

			// ToOrigin과 RightDir의 내적을 확인 (양수면 오른쪽, 음수면 왼쪽)
			if (FVector::DotProduct(ToOrigin, RightDir) > 0.f)
			{
				ForcedTurnDir = EExPathSegmentType::CurveRight;
			}
			else
			{
				ForcedTurnDir = EExPathSegmentType::CurveLeft;
			}
			
			UE_LOG(LogExPathManager, Warning, TEXT("[Bounding Box] 월드 한계 도달 예측! 강제 커브 발생. Dir: %s"), 
				(ForcedTurnDir == EExPathSegmentType::CurveRight) ? TEXT("Right") : TEXT("Left"));
		}
	}

	if (!bShouldCurve)
	{
		// ── 직선 세그먼트 ──
		Segment.Type = EExPathSegmentType::Straight;
		Segment.ArcLength = 1000.f;
		Segment.CurveAngle = 0.f;
		Segment.CurveRadius = 0.f;
		Segment.HeightOffset = 0.f;

		ConsecutiveStraightCount++;
		
		LastTurnType = EExPathSegmentType::Straight;
		ConsecutiveTurnCount = 0;
		CurrentPitch = 0.f;

		return Segment;
	}

	// ── 90도 커브 세그먼트 (Quadrant) ──
	ConsecutiveStraightCount = 0;
	
	if (bForceCurve)
	{
		Segment.Type = ForcedTurnDir;
	}
	else
	{
		Segment.Type = (PathRandomStream.RandRange(0, 1) == 1) ? EExPathSegmentType::CurveLeft : EExPathSegmentType::CurveRight;
	}
	Segment.CurveAngle = 90.f; // 고정 90도
	Segment.CurveRadius = RunnerConfig->Curve.FixedCurveRadius;

	// 호 길이: 2 * PI * R * (90/360) = PI * R * 0.5
	Segment.ArcLength = PI * Segment.CurveRadius * 0.5f;

	// ── 꽈배기 감지 및 Pitch 적용 ──
	if (Segment.Type == LastTurnType)
	{
		ConsecutiveTurnCount++;
	}
	else
	{
		ConsecutiveTurnCount = 1;
		LastTurnType = Segment.Type;
	}

	float TargetPitch = 0.f;

	// 설정된 횟수 이상 같은 방향으로 회전하면 경사 적용
	if (ConsecutiveTurnCount >= RunnerConfig->Curve.SlopeTriggerCount)
	{
		// 상승? 하강?
		// 360도 루프를 피하려면 위나 아래로 보내야 함.
		// 일단 상승(Positive Pitch)으로 고정하거나, 설정에 따라.
		// 보통 나선형 계단은 한쪽으로 계속 올라감.
		TargetPitch = RunnerConfig->Curve.SlopePitchAngle;
	}
	else
	{
		TargetPitch = 0.f;
	}

	// Pitch에 따른 높이 오프셋(Z delta) 계산
	// Height = Length * Tan(Pitch)
	if (!FMath::IsNearlyZero(TargetPitch))
	{
		const float PitchRad = FMath::DegreesToRadians(TargetPitch);
		Segment.HeightOffset = Segment.ArcLength * FMath::Tan(PitchRad);
	}
	else
	{
		Segment.HeightOffset = 0.f;
	}

	UE_LOG(LogExPathManager, Log, TEXT("세그먼트 생성: %s (90도), 연속=%d, Pitch=%.1f, Z-Offset=%.1f"),
		(Segment.Type == EExPathSegmentType::CurveLeft) ? TEXT("좌") : TEXT("우"),
		ConsecutiveTurnCount, TargetPitch, Segment.HeightOffset);

	return Segment;
}

// (CalculateHeightForCurve 제거됨)

// ──────────────────────────────────────────────
// 누적 거리 → 세그먼트 인덱스 조회
// ──────────────────────────────────────────────
int32 UExPathManager::FindSegmentIndexAtDistance(float Distance) const
{
	for (int32 i = 0; i < PathSegments.Num(); ++i)
	{
		const FExPathSegment& Seg = PathSegments[i];
		const float SegEnd = Seg.CumulativeStartDistance + Seg.ArcLength;

		if (Distance >= Seg.CumulativeStartDistance && Distance <= SegEnd)
		{
			return i;
		}
	}

	// 범위 밖 처리
	if (Distance < PathSegments[0].CumulativeStartDistance)
	{
		return 0; // 시작 전이면 첫 세그먼트
	}
	
	// 루프에서 못 찾았으면 마지막 (또는 범위 초과)
	return PathSegments.Num() - 1;
}

// ──────────────────────────────────────────────
// 누적 거리 → 월드 위치
// ──────────────────────────────────────────────
FVector UExPathManager::GetPositionAtDistance(float Distance) const
{
	if (PathSegments.Num() == 0) return FVector::ZeroVector;

	const int32 Idx = FindSegmentIndexAtDistance(Distance);
	if (!PathSegments.IsValidIndex(Idx)) return FVector::ZeroVector;

	const FExPathSegment& Seg = PathSegments[Idx];
	const float LocalDist = Distance - Seg.CumulativeStartDistance;
	const float Alpha = (Seg.ArcLength > 0.f) ? FMath::Clamp(LocalDist / Seg.ArcLength, 0.f, 1.f) : 0.f;

	return Seg.GetPositionAtAlpha(Alpha);
}

// ──────────────────────────────────────────────
// 누적 거리 → 진행 방향
// ──────────────────────────────────────────────
FRotator UExPathManager::GetDirectionAtDistance(float Distance) const
{
	if (PathSegments.Num() == 0) return FRotator::ZeroRotator;

	const int32 Idx = FindSegmentIndexAtDistance(Distance);
	if (!PathSegments.IsValidIndex(Idx)) return FRotator::ZeroRotator;

	const FExPathSegment& Seg = PathSegments[Idx];
	const float LocalDist = Distance - Seg.CumulativeStartDistance;
	const float Alpha = (Seg.ArcLength > 0.f) ? FMath::Clamp(LocalDist / Seg.ArcLength, 0.f, 1.f) : 0.f;

	return Seg.GetRotationAtAlpha(Alpha);
}

// ──────────────────────────────────────────────
// 커브 구간 판정
// ──────────────────────────────────────────────
bool UExPathManager::IsInCurveSection(float Distance) const
{
	return GetSegmentTypeAtDistance(Distance) != EExPathSegmentType::Straight;
}

EExPathSegmentType UExPathManager::GetSegmentTypeAtDistance(float Distance) const
{
	if (PathSegments.Num() == 0) return EExPathSegmentType::Straight;

	const int32 Idx = FindSegmentIndexAtDistance(Distance);
	if (PathSegments.IsValidIndex(Idx))
	{
		return PathSegments[Idx].Type;
	}
	return EExPathSegmentType::Straight;
}

float UExPathManager::GetClosestDistanceAtLocation(const FVector& WorldLocation, float ApproximateDistance, float SearchRadius) const
{
	// 검색 최적화를 위한 2단계 정밀도 검색 (Coarse -> Fine)
	
	float BestDistance = ApproximateDistance;
	float MinSqDist = FLT_MAX;

	// 1단계: 대략적인 범위 검색 (100cm 단위)
	const float CoarseStep = 100.f;
	const float StartDist = FMath::Max(0.f, ApproximateDistance - SearchRadius);
	const float EndDist = FMath::Min(GetTotalPathLength(), ApproximateDistance + SearchRadius);

	for (float D = StartDist; D <= EndDist; D += CoarseStep)
	{
		FVector PT = GetPositionAtDistance(D);
		float SqDist = FVector::DistSquared(PT, WorldLocation);
		if (SqDist < MinSqDist)
		{
			MinSqDist = SqDist;
			BestDistance = D;
		}
	}

	// 2단계: 정밀 검색 (10cm 단위) - 최적 위치 주변 100cm 검색
	const float FineStep = 10.f;
	const float FineRadius = CoarseStep;
	const float FineStart = FMath::Max(0.f, BestDistance - FineRadius);
	const float FineEnd = FMath::Min(GetTotalPathLength(), BestDistance + FineRadius);

	for (float D = FineStart; D <= FineEnd; D += FineStep)
	{
		FVector PT = GetPositionAtDistance(D);
		float SqDist = FVector::DistSquared(PT, WorldLocation);
		if (SqDist < MinSqDist)
		{
			MinSqDist = SqDist;
			BestDistance = D;
		}
	}

	return BestDistance;
}

// ──────────────────────────────────────────────
// 월드 시프트 보정
// ──────────────────────────────────────────────
void UExPathManager::ShiftPathOrigin(const FVector& DeltaOffset)
{
	for (FExPathSegment& Seg : PathSegments)
	{
		Seg.StartWorldPos += DeltaOffset;
		Seg.EndWorldPos += DeltaOffset;
	}
}

float UExPathManager::GetTotalPathLength() const
{
	if (PathSegments.Num() == 0) return 0.f;

	const FExPathSegment& LastSeg = PathSegments.Last();
	return LastSeg.CumulativeStartDistance + LastSeg.ArcLength;
}

void UExPathManager::CleanupSegmentsBefore(float MinDistance)
{
	while (PathSegments.Num() > 1) // 최소 1개 유지
	{
		const FExPathSegment& First = PathSegments[0];
		const float SegEnd = First.CumulativeStartDistance + First.ArcLength;

		if (SegEnd < MinDistance)
		{
			PathSegments.RemoveAt(0);
		}
		else
		{
			break;
		}
	}
}

// ──────────────────────────────────────────────
// [User Request Implementation] Circular Arc Math Verification
// ──────────────────────────────────────────────
FTransform UExPathManager::GetPoseOnCurve(float DistanceTraveled)
{
	// 1. Clamp Input (0 ~ 1000)
	DistanceTraveled = FMath::Clamp(DistanceTraveled, 0.f, 1000.f);

	// 2. Constants for 90-degree turn over 1000 units
	// ArcLength L = 1000
	// TotalAngle = 90 deg = PI/2 rad
	// L = R * Theta -> R = L / Theta = 1000 / (PI/2) = 2000 / PI
	const float Radius = 2000.f / UE_PI; // 약 636.6197f

	// Curvature Rate k = 90 / 1000 = 0.09 deg/unit
	const float YawDeg = DistanceTraveled * 0.09f;
	const float YawRad = FMath::DegreesToRadians(YawDeg);

	// 3. Calculate Relative Location (X, Y)
	// X = R * sin(psi)
	// Y = R * (1 - cos(psi))
	float X = Radius * FMath::Sin(YawRad);
	float Y = Radius * (1.f - FMath::Cos(YawRad));

	// 4. Construct Output
	FVector Location(X, Y, 0.f);
	FRotator Rotation(0.f, YawDeg, 0.f);

	return FTransform(Rotation, Location, FVector::OneVector);
}
