#include "ExObstacleStrategy_Climb.h"
#include "../Actors/ExFloorChunk.h"
#include "ExObstacleDefinition.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineComponent.h"



void UExObstacleStrategy_Climb::ConfigureObstacle_Implementation(
	AActor* Obstacle,
	const UExObstacleDefinition* Def,
	AExFloorChunk* Chunk)
{
	if (!Obstacle || !Def) return;

	// 1. 타겟 크기 초기 랜덤 결정
	float TargetLength = FMath::RandRange(Def->MinSize.X, Def->MaxSize.X);
	float TargetHeight = FMath::RandRange(Def->MinSize.Z, Def->MaxSize.Z);
	float TargetWidth = 1000.f;

	// [버그 수정] AC_TraversalLogic (파쿠르 시스템) 호환성을 위한 두께(Depth/X) 보정 로직
	// - Vault/Hurdle (훌쩍 뛰어넘기): 높이가 낮을 때 발동. 장애물이 너무 두꺼우면 넘지 못하므로 얇게 강제.
	// - Mantle (기어올라 서기): 높이가 높을 때 발동. 옥상 공간(Back Ledge)이 필요하므로 최소 두께 보장.
	constexpr float VaultMaxHeight = 110.0f; // 뛰어넘기 허용 최대 높이 (대략 110cm)
	constexpr float MaxVaultDepth = 30.0f;   // 뛰어넘기 시 허용되는 최대 두께 (다리가 걸리지 않도록)
	constexpr float MinMantleDepth = 80.0f;  // 올라서기 시 보장되어야 하는 최소 두께 (안정적인 옥상 확보)

	if (TargetHeight <= VaultMaxHeight)
	{
		// 낮아서 Vault/Hurdle이 발동해야 하는 구간 
		// 두께가 너무 두꺼우면 플레이어가 넘지 못하고 충돌 패스가 꼬이므로 MaxVaultDepth 이하로 캡핑
		TargetLength = FMath::Min(TargetLength, MaxVaultDepth);
	}
	else
	{
		// 너무 높아서 반드시 옥상에 올라서야(Mantle) 하는 구간
		// Back Ledge Raycast가 옥상에 안정적으로 꽂히도록 최소 두께(MinMantleDepth) 보장
		if (TargetLength < MinMantleDepth)
		{
			TargetLength = FMath::RandRange(MinMantleDepth, FMath::Max(MinMantleDepth, static_cast<float>(Def->MaxSize.X)));
		}
	}

	// ★ 바닥 너비: 로컬 Bounds 기반 (회전에 의한 월드 AABB 왜곡 방지)
	if (Chunk)
	{
		TargetWidth = GetFloorWidth(Chunk);
	}

	// 2. 통합된 스케일 계산 적용
	// [버그 수정] 원본 에셋(Static Mesh)의 로컬 Bounds를 그대로 반환 플래그 추가
	FVector TargetSize(TargetLength, TargetWidth, TargetHeight);
	FVector NewScale = CalculateObstacleScale(Obstacle, TargetSize, true);

	// 기존처럼 직관적으로 액터 전체 루트 스케일 적용
	Obstacle->SetActorScale3D(NewScale);
	
	// [핵심 해결책] 런타임에 액터 스케일이 비균등하게 변하면 Spline 내부 캐시가 깨져 
	// AC_TraversalLogic의 Ledge 좌표 연산이 오작동(우측 오프셋)함. 강제로 곡선과 바운드를 갱신!
	TArray<USplineComponent*> Splines;
	Obstacle->GetComponents<USplineComponent>(Splines);
	for (USplineComponent* Spline : Splines)
	{
		Spline->UpdateSpline();
	}

	// 3. 장애물 정보 주입 (Interface)
	FExObstacleInfo Info;
	Info.Type = EExObstacleType::Climb;
	Info.Value = TargetHeight; // Z축 높이
	LastGeneratedInfoValue = TargetHeight;
	ApplyObstacleInfo(Obstacle, Info);
}
