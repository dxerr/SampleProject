/**
 * @file FExPathSegment.cpp
 * @brief 경로 세그먼트 보간 및 좌표 계산 구현
 * 
 * Copyright ExFrameWork. All Rights Reserved.
 */

#include "FExPathSegment.h"

FVector FExPathSegment::GetPositionAtAlpha(float Alpha) const
{
	Alpha = FMath::Clamp(Alpha, 0.f, 1.f);

	if (Type == EExPathSegmentType::Straight)
	{
		// 직선: 선형 보간 + 높이 보간
		FVector Pos = FMath::Lerp(StartWorldPos, EndWorldPos, Alpha);
		Pos.Z = FMath::Lerp(StartWorldPos.Z, StartWorldPos.Z + HeightOffset, Alpha);
		return Pos;
	}

	// 커브: 원호(Arc) 보간
	// 원의 중심(Center)을 기준으로 각도를 보간
	const float AngleRad = FMath::DegreesToRadians(CurveAngle);
	const float CurrentAngle = AngleRad * Alpha;

	// 진행 방향 벡터
	const FVector Forward = StartWorldRot.Vector();
	// 횡 방향 벡터 (좌커브면 왼쪽, 우커브면 오른쪽이 원의 중심)
	const float SideSign = (Type == EExPathSegmentType::CurveLeft) ? -1.f : 1.f;
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal() * SideSign;

	// 원의 중심 = 시작점 + Right * Radius
	const FVector Center = StartWorldPos + Right * CurveRadius;

	// 시작점에서 원 중심으로의 벡터 (반지름 방향)
	const FVector RadialStart = StartWorldPos - Center;

	// 회전 방향 (좌커브: 시계(-), 우커브: 반시계(+)) - Unreal 좌표계 기준
	// 좌커브: Center(-Y) -> Radial(+Y) -> Rotate(-90) -> (+X) (Forward)
	// 우커브: Center(+Y) -> Radial(-Y) -> Rotate(+90) -> (+X) (Forward)
	const float RotSign = (Type == EExPathSegmentType::CurveLeft) ? -1.f : 1.f;

	// Z축 회전으로 원호 위의 점 계산
	const FVector RotatedRadial = RadialStart.RotateAngleAxis(
		FMath::RadiansToDegrees(CurrentAngle) * RotSign, FVector::UpVector);

	FVector Pos = Center + RotatedRadial;

	// 높이 보간
	Pos.Z = StartWorldPos.Z + HeightOffset * Alpha;

	return Pos;
}

FRotator FExPathSegment::GetRotationAtAlpha(float Alpha) const
{
	Alpha = FMath::Clamp(Alpha, 0.f, 1.f);

	if (Type == EExPathSegmentType::Straight)
	{
		// 직선: 방향 유지
		return StartWorldRot;
	}

	// 커브: 접선 방향 = 시작 Yaw + 진행 각도
	// 좌커브: -Yaw (시계 방향 회전... 아님 반시계? Unreal에서 -Yaw는 왼쪽)
	// Right Turn is +Yaw. Left Turn is -Yaw.
	// So Left Curve needs Negative RotSign.
	const float RotSign = (Type == EExPathSegmentType::CurveLeft) ? -1.f : 1.f;
	const float DeltaYaw = CurveAngle * Alpha * RotSign;

	FRotator Result = StartWorldRot;
	Result.Yaw += DeltaYaw;

	// Pitch는 높이 변화에 따라 약간의 경사 적용
	if (!FMath::IsNearlyZero(HeightOffset) && !FMath::IsNearlyZero(ArcLength))
	{
		Result.Pitch = FMath::RadiansToDegrees(FMath::Atan2(HeightOffset, ArcLength));
	}

	return Result;
}

void FExPathSegment::CalculateEndTransform()
{
	if (Type == EExPathSegmentType::Straight)
	{
		// 직선: 시작점 + 진행방향 * 길이
		const FVector Forward = StartWorldRot.Vector();
		EndWorldPos = StartWorldPos + Forward * ArcLength;
		EndWorldPos.Z += HeightOffset;
		EndWorldRot = StartWorldRot;

		// 높이 변화에 따른 Pitch
		if (!FMath::IsNearlyZero(HeightOffset))
		{
			EndWorldRot.Pitch = FMath::RadiansToDegrees(FMath::Atan2(HeightOffset, ArcLength));
		}
	}
	else
	{
		// 커브: 원호 끝 지점 계산
		EndWorldPos = GetPositionAtAlpha(1.f);
		EndWorldRot = GetRotationAtAlpha(1.f);
		// Pitch 리셋 (커브 끝에서 수평으로 복귀)
		EndWorldRot.Pitch = 0.f;
	}
}
