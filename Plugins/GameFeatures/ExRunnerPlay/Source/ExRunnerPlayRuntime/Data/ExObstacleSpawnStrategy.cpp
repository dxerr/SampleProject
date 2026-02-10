// Copyright ExFrameWork. All Rights Reserved.
// 장애물 스폰 전략 베이스 클래스 구현
// 기본 구현은 기존 SpawnObstaclesOnChunk와 동일한 로직을 제공합니다.

#include "ExObstacleSpawnStrategy.h"
#include "../Actors/ExFloorChunk.h"
#include "ExObstacleDefinition.h"
#include "Components/StaticMeshComponent.h"

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
	float TargetWidth = 1000.f;

	// 바닥 너비 구하기 (기존 GetVisualBounds 사용)
	if (Chunk)
	{
		FBoxSphereBounds FloorBounds = GetVisualBoundsOf(Chunk);
		float FloorHalfWidth = FloorBounds.BoxExtent.Y;
		if (FloorHalfWidth < 10.f) FloorHalfWidth = 500.f;
		TargetWidth = FloorHalfWidth * 2.0f;
	}

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
FVector UExObstacleSpawnStrategy::CalculateSpawnPosition_Implementation(
	const UExObstacleDefinition* Def,
	AExFloorChunk* Chunk,
	float SafeStartX)
{
	if (!Chunk || !Def) return FVector::ZeroVector;

	const FVector ChunkLoc = Chunk->GetActorLocation();
	const float SpawnX = SafeStartX + 200.f; // Buffer

	// 바닥 너비로 Y 피벗 오프셋 계산 (기존 GetVisualBounds 사용)
	FBoxSphereBounds FloorBounds = GetVisualBoundsOf(Chunk);
	float FloorHalfWidth = FloorBounds.BoxExtent.Y;
	if (FloorHalfWidth < 10.f) FloorHalfWidth = 500.f;
	float TargetWidth = FloorHalfWidth * 2.0f;

	// Pivot Adjustment: 바닥 너비 절반만큼 Y 오프셋 (기존 로직)
	return FVector(SpawnX, ChunkLoc.Y - (TargetWidth * 0.5f), ChunkLoc.Z);
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
