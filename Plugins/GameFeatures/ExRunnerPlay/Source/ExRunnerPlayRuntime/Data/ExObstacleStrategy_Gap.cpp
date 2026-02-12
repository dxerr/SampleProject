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
	// PathDistance는 Chunk Center 기준이므로 StartDist 계산 시 HalfLength를 뻼
	const float ChunkStartDist = Chunk->PathDistance - (ChunkLength * 0.5f);

	// 로컬 거리 (ArcLength 기준)
	float LocalDist = (SafeStartX + 200.f) - ChunkStartDist;

	// 커브 로컬 변환 계산
	FTransform CurveTrans = Chunk->GetLocalTransformAtDistance(LocalDist);
	
	// 월드 변환
	FTransform WorldTrans = CurveTrans * Chunk->GetActorTransform();

	// ConfigureObstacle에서 사용할 X 좌표 캐시 (월드 X)
	// Gap 전략은 이 값을 역변환하여 Local X를 구하고, 이를 Gap Slicing에 사용함.
	// 커브의 경우 Local X != ArcLength 이지만, ApplyGap이 StaticMesh Slicing을 하므로 Local X가 맞음.
	CachedSpawnX = WorldTrans.GetLocation().X;

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
	float TargetWidth = 1000.f;
	FBoxSphereBounds FloorBounds = GetVisualBoundsOf_Gap(Chunk);
	float FloorHalfWidth = FloorBounds.BoxExtent.Y;
	if (FloorHalfWidth < 10.f) FloorHalfWidth = 500.f;
	TargetWidth = FloorHalfWidth * 2.0f;

	// 3. 장애물 메시 기본 크기 측정 (스케일 1에서)
	Obstacle->SetActorScale3D(FVector::OneVector);
	Obstacle->UpdateComponentTransforms();

	FBoxSphereBounds ObsBounds = GetVisualBoundsOf_Gap(Obstacle);
	FVector BaseSize = ObsBounds.BoxExtent * 2.0f;
	if (BaseSize.X < 1.f) BaseSize.X = 100.f;
	if (BaseSize.Y < 1.f) BaseSize.Y = 100.f;
	if (BaseSize.Z < 1.f) BaseSize.Z = 100.f;

	// 4. SpawnX (월드) → 청크 로컬 좌표로 변환
	FVector WorldSpawnPos(CachedSpawnX, Chunk->GetActorLocation().Y, Chunk->GetActorLocation().Z);
	FVector LocalPos = Chunk->GetActorTransform().InverseTransformPosition(WorldSpawnPos);
	float GapLocalStartX = LocalPos.X;

	UE_LOG(LogTemp, Log, TEXT("Gap Strategy: Width=%.1f, LocalStartX=%.1f, FloorWidth=%.1f"),
		GapWidth, GapLocalStartX, TargetWidth);

	// ★ 핵심: ChunkFloor에 Gap 적용
	Chunk->ApplyGap(GapLocalStartX, GapWidth);

	// 5. 장애물 스케일 설정
	//    X: Gap 폭, Y: ChunkFloor 너비, Z: 데이터 값
	float TargetHeight = FMath::RandRange(Def->MinSize.Z, Def->MaxSize.Z);
	if (TargetHeight < 1.f) TargetHeight = BaseSize.Z; // Z 미설정 시 기본 유지

	Obstacle->SetActorScale3D(FVector(
		GapWidth     / BaseSize.X,  // X: Gap 폭
		TargetWidth  / BaseSize.Y,  // Y: ChunkFloor 너비
		TargetHeight / BaseSize.Z   // Z: 높이
	));

	// 6. 장애물 정보 주입 (Interface)
	FExObstacleInfo Info;
	Info.Type = EExObstacleType::Gap;
	Info.Value = GapWidth;
	ApplyObstacleInfo(Obstacle, Info);
}
