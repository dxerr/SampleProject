// Copyright ExFrameWork. All Rights Reserved.
// Gap 전략 구현 — ChunkFloor에 구멍을 만들어 점프 장애물 생성
//
// 호출 순서:
// 1. CalculateSpawnPosition() → SpawnX 계산 + CachedSpawnX에 저장
// 2. ConfigureObstacle()      → CachedSpawnX로 ChunkFloor 로컬 좌표 계산 → ApplyGap 호출
// 3. Manager에서 최종 위치 설정

#include "ExObstacleStrategy_Gap.h"
#include "../Actors/ExFloorChunk.h"
#include "ExObstacleDefinition.h"
#include "Components/StaticMeshComponent.h"

// ──────────────────────────────────────────────
// ChunkFloor 바운드 헬퍼 (Slide Strategy와 동일 패턴)
// ──────────────────────────────────────────────
static FBoxSphereBounds GetVisualBoundsOf_Gap(AActor* Actor)
{
	if (!IsValid(Actor))
		return FBoxSphereBounds(FVector::ZeroVector, FVector::ZeroVector, 0.f);

	TArray<UStaticMeshComponent*> MeshComps;
	Actor->GetComponents<UStaticMeshComponent>(MeshComps);
	for (UStaticMeshComponent* Mesh : MeshComps)
	{
		if (Mesh && Mesh->GetStaticMesh())
		{
			return Mesh->Bounds;
		}
	}

	FVector Origin, Extent;
	Actor->GetActorBounds(true, Origin, Extent);
	if (!Extent.IsZero())
	{
		return FBoxSphereBounds(Origin, Extent, Extent.GetMax());
	}

	Actor->GetActorBounds(false, Origin, Extent);
	return FBoxSphereBounds(Origin, Extent, Extent.GetMax());
}

// ──────────────────────────────────────────────
// CalculateSpawnPosition: 바닥 레벨에 배치
// Gap은 높이 오프셋 없음, Y 오프셋 없음 (전체 폭)
// ──────────────────────────────────────────────
// ──────────────────────────────────────────────
// CalculateSpawnPosition: 바닥 레벨에 배치
// Gap은 높이 오프셋 없음, Y 오프셋 없음 (전체 폭)
// ──────────────────────────────────────────────
FTransform UExObstacleStrategy_Gap::CalculateSpawnPosition_Implementation(
	const UExObstacleDefinition* Def,
	AExFloorChunk* Chunk,
	float SafeStartX)
{
	if (!Chunk || !Def) return FTransform::Identity;

	const float ChunkLength = Chunk->ChunkLength;
	const float ChunkStartDist = Chunk->PathDistance - (ChunkLength * 0.5f);

	// 로컬 거리 (ArcLength 기준)
	float LocalDist = (SafeStartX + 200.f) - ChunkStartDist;
	
	// ConfigureObstacle에서 사용할 로컬 시작 거리 캐싱
	CachedSpawnDist = LocalDist;

	// 1. Chunk 중점(Center)에서의 Local Transform (위치 및 방향) 가져오기
	FTransform CenterCurveTrans = Chunk->GetLocalTransformAtDistance(LocalDist);

	// 2. 곡선/경사도에 따른 정확한 회전(Rotation) 계산
	if (!FMath::IsNearlyZero(Chunk->CachedHeightOffset) && !FMath::IsNearlyZero(Chunk->CachedCurveAngle))
	{
		float ObsLen = Def ? Def->MaxSize.X : 500.f;
		if (ObsLen < 10.f) ObsLen = 100.f;

		FVector FrontPos = CenterCurveTrans.GetLocation();
		
		float RearLocalDist = LocalDist + ObsLen;
		float ClampRearDist = FMath::Min(RearLocalDist, Chunk->ChunkLength);
		float OverflowDist = RearLocalDist - ClampRearDist;

		FTransform RearTrans = Chunk->GetLocalTransformAtDistance(ClampRearDist);

		if (OverflowDist > 0.f)
		{
			FVector ForwardDirRear = RearTrans.GetRotation().GetForwardVector();
			RearTrans.AddToTranslation(ForwardDirRear * OverflowDist);
		}

		FVector RearPos = RearTrans.GetLocation();
		FVector DirVec = RearPos - FrontPos;

		FRotator NewLocalRot = CenterCurveTrans.GetRotation().Rotator();
		float Size2D = DirVec.Size2D();
		if (Size2D > KINDA_SMALL_NUMBER)
		{
			// 중앙축의 실제 경사도(Pitch)와 방향(Yaw)
			NewLocalRot.Pitch = FMath::RadiansToDegrees(FMath::Atan2(DirVec.Z, Size2D));
			NewLocalRot.Yaw = FMath::RadiansToDegrees(FMath::Atan2(DirVec.Y, DirVec.X));
		}
		
		CenterCurveTrans.SetRotation(NewLocalRot.Quaternion());
	}

	// 3. 측면 Y Offset Location 계산
	// ★ 회전이 먼저 적용된 Axis의 RightVector를 따라 이동하여 측면 피봇에 맞춰 Y축 이동
	float TargetWidth = GetFloorWidth(Chunk);
	float YOffset = -(TargetWidth * 0.5f); // 장애물 축이 좌측 가장자리라고 가정
	
	FVector RightDir = CenterCurveTrans.GetRotation().GetRightVector();
	CenterCurveTrans.AddToTranslation(RightDir * YOffset);

	// 4. World 변환
	FTransform WorldTrans = CenterCurveTrans * Chunk->GetActorTransform();

	return WorldTrans;
}

// ──────────────────────────────────────────────
// ConfigureObstacle: Gap 폭만큼 ChunkFloor 부분 제거
// ──────────────────────────────────────────────
void UExObstacleStrategy_Gap::ConfigureObstacle_Implementation(
	AActor* Obstacle,
	const UExObstacleDefinition* Def,
	AExFloorChunk* Chunk)
{
	if (!Obstacle || !Def || !Chunk) return;

	// 1. Gap 폭 결정 (X축 랜덤)
	float GapWidth = FMath::RandRange(Def->MinSize.X, Def->MaxSize.X);

	// 2. ChunkFloor 너비 계산 (Y축 스케일 용)
	// ★ 로컬 Bounds 기반 (회전에 의한 월드 AABB 왜곡 방지)
	float TargetWidth = GetFloorWidth(Chunk);

	// 3. 장애물 메시 기본 크기 측정 (스케일 1에서)
	Obstacle->SetActorScale3D(FVector::OneVector);
	Obstacle->UpdateComponentTransforms();

	FBoxSphereBounds ObsBounds = GetVisualBoundsOf_Gap(Obstacle);
	FVector BaseSize = ObsBounds.BoxExtent * 2.0f;
	if (BaseSize.X < 1.f) BaseSize.X = 100.f;
	if (BaseSize.Y < 1.f) BaseSize.Y = 100.f;
	if (BaseSize.Z < 1.f) BaseSize.Z = 100.f;

	// 4. 장애물 시작 위치 (로컬 스플라인 누적 거리 기준, 0 ~ ChunkLength)
	float GapLocalStartDist = CachedSpawnDist;

	// 곡선 보정으로 인해 호 단위 확장이 과하게 적용되지 않도록 논리 거리를 동일하게 사용
	float LogicalGapWidth = GapWidth;

	float MaxAllowedWidth = FMath::Max(0.f, Chunk->ChunkLength - GapLocalStartDist);
	if (LogicalGapWidth > MaxAllowedWidth)
	{
		UE_LOG(LogTemp, Warning, TEXT("Gap Strategy: LogicalGapWidth %.1f exceeded MaxAllowed %.1f. Clamping."), LogicalGapWidth, MaxAllowedWidth);
		LogicalGapWidth = MaxAllowedWidth;
		GapWidth = LogicalGapWidth;
	}

	UE_LOG(LogTemp, Log, TEXT("Gap Strategy: Final PhysicalWidth=%.1f, LogicalWidth=%.1f, LocalStartDist=%.1f"),
		GapWidth, LogicalGapWidth, GapLocalStartDist);

	// ★ 핵심: ChunkFloor에 Gap 적용 (바닥 구멍 내기) - 여기엔 논리적 거리를 전달!
	Chunk->ApplyGap(GapLocalStartDist, LogicalGapWidth);

	float TargetHeight = FMath::RandRange(Def->MinSize.Z, Def->MaxSize.Z);
	if (TargetHeight < 1.f) TargetHeight = BaseSize.Z; // Z 미설정 시 기본 유지

	// 5. 스냅된 거리에 맞게 장애물 Transform 완벽히 재계산
	// Gap 구멍의 정중앙 지점을 찾습니다.
	float CenterDist = GapLocalStartDist + (GapWidth * 0.5f);
	CenterDist = FMath::Clamp(CenterDist, 0.f, Chunk->ChunkLength);

	FTransform CenterTrans = Chunk->GetLocalTransformAtDistance(CenterDist);

	// 높이 단차가 있다면 Pitch 기울기도 중심 기준으로 다시 계산
	if (!FMath::IsNearlyZero(Chunk->CachedHeightOffset))
	{
		float RearDist = FMath::Min(CenterDist + (GapWidth * 0.5f), Chunk->ChunkLength);
		FTransform RearTrans = Chunk->GetLocalTransformAtDistance(RearDist);
		FVector DirVec = RearTrans.GetLocation() - CenterTrans.GetLocation();
		float Size2D = DirVec.Size2D();
		
		if (Size2D > KINDA_SMALL_NUMBER)
		{
			FRotator NewLocalRot = CenterTrans.GetRotation().Rotator();
			NewLocalRot.Pitch = FMath::RadiansToDegrees(FMath::Atan2(DirVec.Z, Size2D));
			CenterTrans.SetRotation(NewLocalRot.Quaternion());
		}
	}

	// 목표물(Gap Trigger Box)을 좌측 가장자리축(YOffset)으로 정렬하고, 
	// 지면 위가 아니라 지면 "아래"로 향하도록 Z축(UpVector 반대)으로 내립니다.
	float YOffset = -(TargetWidth * 0.5f);
	CenterTrans.AddToTranslation(CenterTrans.GetRotation().GetRightVector() * YOffset);
	CenterTrans.AddToTranslation(CenterTrans.GetRotation().GetUpVector() * (-TargetHeight * 0.5f));

	// 최종 완성된 위치로 액터 이동
	Obstacle->SetActorTransform(CenterTrans * Chunk->GetActorTransform());

	// 6. 장애물 액터 자체의 스케일은 건드리지 않음 (BP 설정값 유지)
	// 대신 자식으로 있는 StaticMeshComponent들만 Y축(폭) 스케일을 바닥에 맞춤
	TArray<UStaticMeshComponent*> MeshComps;
	Obstacle->GetComponents<UStaticMeshComponent>(MeshComps);
	for (UStaticMeshComponent* Mesh : MeshComps)
	{
		if (Mesh)
		{
			// Mesh 자체의 로컬 스케일을 가져와서 Y축만 변경 (액터 스케일에 곱해진 최종 크기가 TargetWidth가 되도록)
			FVector CurrentScale = Mesh->GetRelativeScale3D();
			FVector ActorScale = Obstacle->GetActorScale3D();
			
			// BaseSize.Y 는 이미 액터 스케일 1.0 기준의 Mesh Bounds Y 크기임
			if (BaseSize.Y > KINDA_SMALL_NUMBER && ActorScale.Y > KINDA_SMALL_NUMBER)
			{
				float DesiredRelativeScaleY = (TargetWidth / BaseSize.Y) / ActorScale.Y;
				Mesh->SetRelativeScale3D(FVector(CurrentScale.X, DesiredRelativeScaleY, CurrentScale.Z));
			}
		}
	}

	// 6. 장애물 정보 주입 (Interface)
	FExObstacleInfo Info;
	Info.Type = EExObstacleType::Gap;
	Info.Value = GapWidth;
	ApplyObstacleInfo(Obstacle, Info);
}
