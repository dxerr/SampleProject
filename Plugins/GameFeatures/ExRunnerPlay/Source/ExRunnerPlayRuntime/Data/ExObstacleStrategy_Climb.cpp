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

	// [버그 수정] SetActorScale3D를 사용하면 AC_TraversalLogic 내 Spline 거리 계산 노드에서
	// 오차가 발생하여 WarpTarget이 우측으로 지나치게 이동하는 고질적인 버그가 발생함.
	// 따라서 외부 Actor Scale은 1.0으로 고정하고, 내부의 부품 컴포넌트들만 스케일/위치를 조작함.
	Obstacle->SetActorScale3D(FVector::OneVector);

	// 2-1. 기존 컴포넌트 스케일 백업 (오브젝트 풀링 '눈덩이 현상' 방지)
	FVector PreviousScale = FVector::OneVector;
	TArray<UStaticMeshComponent*> MeshComps;
	Obstacle->GetComponents<UStaticMeshComponent>(MeshComps);

	if (MeshComps.Num() > 0 && MeshComps[0] != nullptr)
	{
		PreviousScale = MeshComps[0]->GetRelativeScale3D();
	}

	// 0 나누기 방어
	PreviousScale.X = FMath::IsNearlyZero(PreviousScale.X) ? 1.0f : PreviousScale.X;
	PreviousScale.Y = FMath::IsNearlyZero(PreviousScale.Y) ? 1.0f : PreviousScale.Y;
	PreviousScale.Z = FMath::IsNearlyZero(PreviousScale.Z) ? 1.0f : PreviousScale.Z;

	// 방금 전(PreviousScale) 대비 이번 스폰에서 곱해야 할 '추가 비율 계수'
	FVector ScaleMultiplier(ScaleX / PreviousScale.X, ScaleY / PreviousScale.Y, ScaleZ / PreviousScale.Z);

	// 2-2. StaticMeshComponent 스케일 직접 변경
	for (UStaticMeshComponent* MeshComp : MeshComps)
	{
		MeshComp->SetRelativeScale3D(FVector(ScaleX, ScaleY, ScaleZ));
	}

	// 2-3. SplineComponent 포인트 위치 스케일 적용 및 이중 스케일 방지
	TArray<USplineComponent*> Splines;
	Obstacle->GetComponents<USplineComponent>(Splines);
	for (USplineComponent* Spline : Splines)
	{
		// [핵심] SplineComponent가 StaticMeshComponent의 자식인 경우, 
		// 부모의 스케일이 그대로 상속됨! 언리얼 계산 노드 버그를 피하려면 스플라인 자체의 월드 스케일은 1.0이어야 함.
		// 따라서 부모 스케일의 역수를 구해서 로컬 스케일로 적용하여 상쇄함.
		float SafeScaleX = FMath::IsNearlyZero(ScaleX) ? 1.0f : ScaleX;
		float SafeScaleY = FMath::IsNearlyZero(ScaleY) ? 1.0f : ScaleY;
		float SafeScaleZ = FMath::IsNearlyZero(ScaleZ) ? 1.0f : ScaleZ;
		
		Spline->SetRelativeScale3D(FVector(1.0f / SafeScaleX, 1.0f / SafeScaleY, 1.0f / SafeScaleZ));

		int32 NumPoints = Spline->GetNumberOfSplinePoints();
		for (int32 i = 0; i < NumPoints; ++i)
		{
			FVector OrigLocalPos = Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);
			// 스플라인 포인트를 이전 풀링 상태 대비 현재 확대되어야 할 비율만큼만 조절 (눈덩이 현상 해결)
			FVector ScaledLocalPos = OrigLocalPos * ScaleMultiplier;
			Spline->SetLocationAtSplinePoint(i, ScaledLocalPos, ESplineCoordinateSpace::Local, true);
		}
		Spline->UpdateSpline();
	}

	// 3. 장애물 정보 주입 (Interface)
	FExObstacleInfo Info;
	Info.Type = EExObstacleType::Climb;
	Info.Value = TargetHeight; // Z축 높이
	ApplyObstacleInfo(Obstacle, Info);
}
