#include "ExObstacleStrategy_WallRun.h"
#include "../Actors/ExFloorChunk.h"
#include "ExObstacleDefinition.h"
#include "Components/StaticMeshComponent.h"



void UExObstacleStrategy_WallRun::ConfigureObstacle_Implementation(
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

	// 2. 통합된 스케일 계산 적용
	FVector TargetSize(TargetLength, TargetWidth, TargetHeight);
	FVector NewScale = CalculateObstacleScale(Obstacle, TargetSize);
	Obstacle->SetActorScale3D(NewScale);

	// 3. 장애물 정보 주입 (Interface)
	FExObstacleInfo Info;
	Info.Type = EExObstacleType::WallRun;
	Info.Value = TargetLength; // X축 길이(벽 길이)
	LastGeneratedInfoValue = TargetLength;
	ApplyObstacleInfo(Obstacle, Info);
}
