#include "ExObstacleStrategy_Climb.h"
#include "../Actors/ExFloorChunk.h"
#include "ExObstacleDefinition.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineComponent.h"

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
			// [버그 수정] 오브젝트 풀링 시 이전 스케일 버그 및 회전에 의한 AABB 왜곡 제거를 위해,
			// 원본 에셋(Static Mesh)의 로컬 Bounds를 그대로 반환합니다. 
			// 이로써 장애물이 커브 위에서 임의의 회전을 가지더라도 항상 정확하고 순수한 1.0 기준 사이즈를 가져옵니다.
			return Mesh->GetStaticMesh()->GetBounds();
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

	// ★ 바닥 너비: 로컬 Bounds 기반 (회전에 의한 월드 AABB 왜곡 방지)
	if (Chunk)
	{
		TargetWidth = GetFloorWidth(Chunk);
	}

	// 2. 스케일 적용
	Obstacle->SetActorScale3D(FVector::OneVector);
	Obstacle->UpdateComponentTransforms();

	FBoxSphereBounds ObsBounds = GetVisualBoundsOf_Climb(Obstacle);
	FVector BaseSize = ObsBounds.BoxExtent * 2.0f;

	if (BaseSize.X < 1.f) BaseSize.X = 100.f;
	if (BaseSize.Y < 1.f) BaseSize.Y = 100.f;
	if (BaseSize.Z < 1.f) BaseSize.Z = 100.f;

	float ScaleX = TargetLength / BaseSize.X;
	float ScaleY = TargetWidth / BaseSize.Y;
	float ScaleZ = TargetHeight / BaseSize.Z;

	// 기존처럼 직관적으로 액터 전체 루트 스케일 적용으로 원복
	Obstacle->SetActorScale3D(FVector(ScaleX, ScaleY, ScaleZ));

	// 3. 장애물 정보 주입 (Interface)
	FExObstacleInfo Info;
	Info.Type = EExObstacleType::Climb;
	Info.Value = TargetHeight; // Z축 높이
	ApplyObstacleInfo(Obstacle, Info);
}
