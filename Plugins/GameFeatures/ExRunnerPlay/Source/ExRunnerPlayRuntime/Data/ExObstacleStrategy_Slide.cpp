// Copyright ExFrameWork. All Rights Reserved.
// Slide 전략 구현 — 천장형 장애물, Crouch/Slide로 아래 공간을 통과
// 캐릭터의 Mover StanceSettings에서 CrouchHalfHeight를 런타임으로 읽어
// 장애물 하단 높이를 결정합니다.

#include "ExObstacleStrategy_Slide.h"
#include "../Actors/ExFloorChunk.h"
#include "ExObstacleDefinition.h"
#include "Components/StaticMeshComponent.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/Settings/StanceSettings.h"
#include "Kismet/GameplayStatics.h"

// ──────────────────────────────────────────────
// 기존 GetVisualBounds와 동일한 헬퍼
// ──────────────────────────────────────────────
static FBoxSphereBounds GetVisualBoundsOf_Slide(AActor* Actor)
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
// 캐릭터의 Mover StanceSettings에서 CrouchHalfHeight 읽기
// Fallback: UExObstacleDefinition::MaxPassHeight
// ──────────────────────────────────────────────
static float GetCrouchPassHeight(const UObject* WorldContext, float Margin)
{
	// Fallback: StanceSettings를 못 찾을 경우 (Crouch 전체 높이 기본 추정)
	constexpr float DefaultPassHeight = 120.f;

	if (!WorldContext) return DefaultPassHeight;

	// 플레이어 Pawn 찾기
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(WorldContext, 0);
	if (!PlayerPawn) return DefaultPassHeight;

	// Pawn에서 UMoverComponent 찾기
	UMoverComponent* MoverComp = PlayerPawn->FindComponentByClass<UMoverComponent>();
	if (!MoverComp) return DefaultPassHeight;

	// StanceSettings에서 CrouchHalfHeight 읽기
	const UStanceSettings* StanceSettings = MoverComp->FindSharedSettings<UStanceSettings>();
	if (!StanceSettings) return DefaultPassHeight;

	// CrouchHalfHeight는 반높이(HalfHeight)이므로 * 2 = 전체 높이
	// + Margin = 여유 마진 (에디터 조정 가능)
	float CrouchFullHeight = StanceSettings->CrouchHalfHeight * 2.0f;
	float PassHeight = CrouchFullHeight + Margin;

	UE_LOG(LogTemp, Verbose, TEXT("Slide: CrouchHalfHeight=%.1f → PassHeight=%.1f"),
		StanceSettings->CrouchHalfHeight, PassHeight);

	return PassHeight;
}

// ──────────────────────────────────────────────
// ConfigureObstacle: Slide 전용 스케일 설정
// X: 두께(Depth) = MinSize.X ~ MaxSize.X 랜덤
// Y: 바닥 폭 = ChunkFloor 너비에 맞춤
// Z: 높이(Height) = MinSize.Z ~ MaxSize.Z 랜덤
// ──────────────────────────────────────────────
void UExObstacleStrategy_Slide::ConfigureObstacle_Implementation(
	AActor* Obstacle,
	const UExObstacleDefinition* Def,
	AExFloorChunk* Chunk)
{
	if (!Obstacle || !Def) return;

	// 1. 타겟 크기 결정
	float TargetDepth  = FMath::RandRange(Def->MinSize.X, Def->MaxSize.X);
	float TargetHeight = FMath::RandRange(Def->MinSize.Z, Def->MaxSize.Z);
	float TargetWidth  = 1000.f;

	// Y: 바닥 너비에 맞춤
	if (Chunk)
	{
		FBoxSphereBounds FloorBounds = GetVisualBoundsOf_Slide(Chunk);
		float FloorHalfWidth = FloorBounds.BoxExtent.Y;
		if (FloorHalfWidth < 10.f) FloorHalfWidth = 500.f;
		TargetWidth = FloorHalfWidth * 2.0f;
	}

	// 2. 기본 메시 크기 측정
	Obstacle->SetActorScale3D(FVector::OneVector);
	Obstacle->UpdateComponentTransforms();

	FBoxSphereBounds ObsBounds = GetVisualBoundsOf_Slide(Obstacle);
	FVector BaseSize = ObsBounds.BoxExtent * 2.0f;

	if (BaseSize.X < 1.f) BaseSize.X = 100.f;
	if (BaseSize.Y < 1.f) BaseSize.Y = 100.f;
	if (BaseSize.Z < 1.f) BaseSize.Z = 100.f;

	// 3. 최종 스케일 적용
	Obstacle->SetActorScale3D(FVector(
		TargetDepth  / BaseSize.X,  // X: 두께
		TargetWidth  / BaseSize.Y,  // Y: 바닥 폭
		TargetHeight / BaseSize.Z   // Z: 장애물 높이
	));

	// 4. 장애물 정보 주입 (Interface)
	FExObstacleInfo Info;
	Info.Type = EExObstacleType::Slide;
	Info.Value = TargetDepth; // X축 길이(두께)
	ApplyObstacleInfo(Obstacle, Info);
}

// ──────────────────────────────────────────────
// CalculateSpawnPosition: Slide 전용 위치 계산
// Z = Floor.Z + CrouchPassHeight (런타임 캐릭터 데이터 기반)
// ──────────────────────────────────────────────
FVector UExObstacleStrategy_Slide::CalculateSpawnPosition_Implementation(
	const UExObstacleDefinition* Def,
	AExFloorChunk* Chunk,
	float SafeStartX)
{
	if (!Chunk || !Def) return FVector::ZeroVector;

	const FVector ChunkLoc = Chunk->GetActorLocation();
	const float SpawnX = SafeStartX + 200.f;

	// Y: 바닥 너비로 피벗 오프셋 (기본 로직)
	FBoxSphereBounds FloorBounds = GetVisualBoundsOf_Slide(Chunk);
	float FloorHalfWidth = FloorBounds.BoxExtent.Y;
	if (FloorHalfWidth < 10.f) FloorHalfWidth = 500.f;
	float TargetWidth = FloorHalfWidth * 2.0f;

	// ★ Z: 실제 캐릭터의 CrouchHalfHeight + 여유 마진으로 통과 높이 계산
	float PassHeight = GetCrouchPassHeight(Chunk, ClearanceMargin);

	return FVector(SpawnX, ChunkLoc.Y - (TargetWidth * 0.5f), ChunkLoc.Z + PassHeight);
}
