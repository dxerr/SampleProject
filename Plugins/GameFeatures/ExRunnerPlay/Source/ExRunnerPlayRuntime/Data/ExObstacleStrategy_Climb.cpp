#include "ExObstacleStrategy_Climb.h"
#include "../Actors/ExFloorChunk.h"
#include "ExObstacleDefinition.h"
#include "Components/StaticMeshComponent.h"

// ──────────────────────────────────────────────
// Base Strategy의 헬퍼 함수 복사 (static)
// ──────────────────────────────────────────────
static FBoxSphereBounds GetVisualBoundsOf_Climb(AActor* Actor)
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
	if (!Extent.IsZero()) {
		return FBoxSphereBounds(Origin, Extent, Extent.GetMax());
	}
	Actor->GetActorBounds(false, Origin, Extent);
	return FBoxSphereBounds(Origin, Extent, Extent.GetMax());
}

void UExObstacleStrategy_Climb::ConfigureObstacle_Implementation(
	AActor* Obstacle,
	const UExObstacleDefinition* Def,
	AExFloorChunk* Chunk)
{
	if (!Obstacle || !Def) return;

	// 1. 타겟 크기 결정
	float TargetLength = FMath::RandRange(Def->MinSize.X, Def->MaxSize.X);
	float TargetHeight = FMath::RandRange(Def->MinSize.Z, Def->MaxSize.Z);
	float TargetWidth = 1000.f;

	// 바닥 너비 구하기
	if (Chunk)
	{
		FBoxSphereBounds FloorBounds = GetVisualBoundsOf_Climb(Chunk);
		float FloorHalfWidth = FloorBounds.BoxExtent.Y;
		if (FloorHalfWidth < 10.f) FloorHalfWidth = 500.f;
		TargetWidth = FloorHalfWidth * 2.0f;
	}

	// 2. 스케일 적용
	Obstacle->SetActorScale3D(FVector::OneVector);
	Obstacle->UpdateComponentTransforms();

	FBoxSphereBounds ObsBounds = GetVisualBoundsOf_Climb(Obstacle);
	FVector BaseSize = ObsBounds.BoxExtent * 2.0f;

	if (BaseSize.X < 1.f) BaseSize.X = 100.f;
	if (BaseSize.Y < 1.f) BaseSize.Y = 100.f;
	if (BaseSize.Z < 1.f) BaseSize.Z = 100.f;

	Obstacle->SetActorScale3D(FVector(
		TargetLength / BaseSize.X,
		TargetWidth / BaseSize.Y,
		TargetHeight / BaseSize.Z
	));

	// 3. 장애물 정보 주입 (Interface)
	FExObstacleInfo Info;
	Info.Type = EExObstacleType::Climb;
	Info.Value = TargetHeight; // Z축 높이
	ApplyObstacleInfo(Obstacle, Info);
}
