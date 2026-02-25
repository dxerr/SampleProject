// Copyright ExFrameWork. All Rights Reserved.
// 장애물 스폰 전략 베이스 클래스 구현
// 기본 구현은 기존 SpawnObstaclesOnChunk와 동일한 로직을 제공합니다.

#include "ExObstacleSpawnStrategy.h"
#include "../Actors/ExFloorChunk.h"
#include "ExObstacleDefinition.h"
#include "Components/StaticMeshComponent.h"
#include "../Interfaces/ExObstacleInterface.h"

// ──────────────────────────────────────────────
// 기존 UExObstacleManager::GetVisualBounds와 동일한 헬퍼
// Collision(BoxComponent 등)을 제외하고 순수 비주얼 메시의 Bounds만 반환
// ──────────────────────────────────────────────
static FBoxSphereBounds GetVisualBoundsOf(AActor* Actor)
{
	if (!IsValid(Actor))
		return FBoxSphereBounds(FVector::ZeroVector, FVector::ZeroVector, 0.f);

	// 1. StaticMeshComponent의 Bounds 직접 사용 (가장 정확)
	TArray<UStaticMeshComponent*> MeshComps;
	Actor->GetComponents<UStaticMeshComponent>(MeshComps);
	for (UStaticMeshComponent* Mesh : MeshComps)
	{
		if (Mesh && Mesh->GetStaticMesh())
		{
			return Mesh->Bounds; // 월드 공간 Bounds
		}
	}

	// 2. Fallback: Colliding Components
	FVector Origin, Extent;
	Actor->GetActorBounds(true, Origin, Extent);
	if (!Extent.IsZero())
	{
		return FBoxSphereBounds(Origin, Extent, Extent.GetMax());
	}

	// 3. Last Resort: All Components
	Actor->GetActorBounds(false, Origin, Extent);
	return FBoxSphereBounds(Origin, Extent, Extent.GetMax());
}

// ──────────────────────────────────────────────
// FloorChunk의 실제 바닥 폭(Y축)을 로컬 Bounds 기반으로 반환
// ★ Mesh->Bounds(월드 AABB)는 커브 청크 회전 시 왜곡되므로,
//   로컬 메시 Bounds × 컴포넌트 스케일을 사용하여 정확한 폭 계산
// ──────────────────────────────────────────────
float UExObstacleSpawnStrategy::GetFloorWidth(const AExFloorChunk* Chunk)
{
	if (!Chunk || !Chunk->FloorMesh || !Chunk->FloorMesh->GetStaticMesh())
	{
		return 1000.f; // Fallback 기본값
	}

	// 로컬 메시 Bounds (회전 영향 없음)
	FBoxSphereBounds LocalBounds = Chunk->FloorMesh->GetStaticMesh()->GetBounds();
	FVector MeshScale = Chunk->FloorMesh->GetRelativeScale3D();

	// 실제 폭 = 로컬 반폭 × Y스케일 × 2
	float FloorWidth = LocalBounds.BoxExtent.Y * FMath::Abs(MeshScale.Y) * 2.0f;

	if (FloorWidth < 10.f) FloorWidth = 1000.f; // 안전 Fallback
	return FloorWidth;
}

// ──────────────────────────────────────────────
// 기존 로직과 동일: 장애물 스케일/크기 설정
// ──────────────────────────────────────────────
void UExObstacleSpawnStrategy::ConfigureObstacle_Implementation(
	AActor* Obstacle,
	const UExObstacleDefinition* Def,
	AExFloorChunk* Chunk)
{
	if (!Obstacle || !Def) return;

	// 1. 랜덤 크기 생성
	float TargetLength = FMath::RandRange(Def->MinSize.X, Def->MaxSize.X);
	float TargetHeight = FMath::RandRange(Def->MinSize.Z, Def->MaxSize.Z);

	// ★ 바닥 너비: 로컬 Bounds 기반 (회전에 의한 월드 AABB 왜곡 방지)
	float TargetWidth = GetFloorWidth(Chunk);

	// 2. 스케일 적용 (기존 스케일 초기화 후 메시 기본 크기 측정)
	Obstacle->SetActorScale3D(FVector::OneVector);
	Obstacle->UpdateComponentTransforms();

	// ★ 핵심: Collision(BoxComponent) 아닌 StaticMesh의 Bounds 사용
	// 풀 재활용 시 BoxComponent.Extent가 이전 값을 유지하므로
	// GetActorBounds(true) 대신 GetVisualBoundsOf 사용
	FBoxSphereBounds ObsBounds = GetVisualBoundsOf(Obstacle);
	FVector BaseSize = ObsBounds.BoxExtent * 2.0f;

	if (BaseSize.X < 1.f) BaseSize.X = 100.f;
	if (BaseSize.Y < 1.f) BaseSize.Y = 100.f;
	if (BaseSize.Z < 1.f) BaseSize.Z = 100.f;

	Obstacle->SetActorScale3D(FVector(
		TargetLength / BaseSize.X,
		TargetWidth / BaseSize.Y,
		TargetHeight / BaseSize.Z
	));
}

// ──────────────────────────────────────────────
// 기존 로직과 동일: 스폰 위치 계산 (Pivot Adjustment 포함)
// ──────────────────────────────────────────────
// ──────────────────────────────────────────────
// 기존 로직과 수정된 커브 로직 분기: 스폰 위치 및 회전 계산
// ──────────────────────────────────────────────
FTransform UExObstacleSpawnStrategy::CalculateSpawnPosition_Implementation(
	const UExObstacleDefinition* Def,
	AExFloorChunk* Chunk,
	float SafeStartX)
{
	if (!Chunk || !Def) return FTransform::Identity;

	const float ChunkLength = Chunk->ChunkLength;
	const float ChunkStartDist = Chunk->PathDistance - (ChunkLength * 0.5f);
	
	// 로컬 기준 (중심점) 거리
	float LocalDist = (SafeStartX + 200.f) - ChunkStartDist;

	// 1. Chunk 중점(Center)에서의 Local Transform (순수한 높이 Location 계산)
	FTransform CenterCurveTrans = Chunk->GetLocalTransformAtDistance(LocalDist);

	// 2. 곡선/경사도에 따른 회전(Rotation) 계산
	if (!FMath::IsNearlyZero(Chunk->CachedHeightOffset) && !FMath::IsNearlyZero(Chunk->CachedCurveAngle))
	{
		float ObsLen = Def ? Def->MaxSize.X : 500.f; // 장애물의 길이
		if (ObsLen < 10.f) ObsLen = 100.f;

		// 중앙축 앞/끝 계산
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

		// 현(Chord) 벡터 (앞점 -> 뒷점)
		FVector DirVec = RearPos - FrontPos;

		FRotator NewLocalRot = CenterCurveTrans.GetRotation().Rotator();
		float Size2D = DirVec.Size2D();
		
		if (Size2D > KINDA_SMALL_NUMBER)
		{
			// 중앙축의 실제 경사도(Pitch)와 방향(Yaw)
			NewLocalRot.Pitch = FMath::RadiansToDegrees(FMath::Atan2(DirVec.Z, Size2D));
			NewLocalRot.Yaw = FMath::RadiansToDegrees(FMath::Atan2(DirVec.Y, DirVec.X));
		}
		
		// 회전을 먼저 완벽히 적용
		CenterCurveTrans.SetRotation(NewLocalRot.Quaternion());
	}

	// 3. 측면 X, Y Offset Location 계산
	// ★ 회전이 먼저 적용된 Axis(Pitch 포함)의 RightVector를 따라 이동해야
	// 측면으로 빗겨날 때에도 바닥 경사면을 따라 대각선으로 자연스럽게 고도가 유지됩니다.
	float TargetWidth = GetFloorWidth(Chunk);
	float YOffset = -(TargetWidth * 0.5f); // 장애물 축이 좌측 가장자리라고 가정
	
	FVector RightDir = CenterCurveTrans.GetRotation().GetRightVector();
	CenterCurveTrans.AddToTranslation(RightDir * YOffset);

	// 4. World 변환 (Chunk Actor Transform 적용)
	FTransform WorldTrans = CenterCurveTrans * Chunk->GetActorTransform();

	return WorldTrans;
}

// ──────────────────────────────────────────────
// 기본 복귀 거리: RecoveryTime * RunSpeed (기존 로직과 동일)
// ──────────────────────────────────────────────
float UExObstacleSpawnStrategy::GetRecoveryDistance_Implementation(
	const UExObstacleDefinition* Def,
	float RunSpeed)
{
	if (!Def) return 200.f;
	return Def->RecoveryTime * RunSpeed;
}

// ──────────────────────────────────────────────
// 장애물 정보 주입 헬퍼 (Interface 호출)
// ──────────────────────────────────────────────
void UExObstacleSpawnStrategy::ApplyObstacleInfo(AActor* Obstacle, const FExObstacleInfo& Info)
{
	if (!IsValid(Obstacle))
	{
		return;
	}

	// 인터페이스 구현 여부 확인
	if (Obstacle->GetClass()->ImplementsInterface(UExObstacleInterface::StaticClass()))
	{
		// 단위 변환: cm -> m, 소수점 2자리 반올림
		FExObstacleInfo ModifiedInfo = Info;
		float MeterValue = Info.Value * 0.01f; // / 100.0f
		ModifiedInfo.Value = FMath::RoundToFloat(MeterValue * 100.0f) / 100.0f;

		// BlueprintNativeEvent 호출 (Execute_ 접두사 사용)
		IExObstacleInterface::Execute_SetupObstacleInfo(Obstacle, ModifiedInfo);
	}
}

