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
// Z: 두께(Height) = 원본 크기 대비 1.0 ~ 1.5배 랜덤
// ──────────────────────────────────────────────
void UExObstacleStrategy_Slide::ConfigureObstacle_Implementation(
	AActor* Obstacle,
	const UExObstacleDefinition* Def,
	AExFloorChunk* Chunk)
{
	if (!Obstacle || !Def) return;

	// 1. 타겟 크기 결정
	float TargetDepth  = FMath::RandRange(Def->MinSize.X, Def->MaxSize.X);
	float TargetWidth  = 1000.f;

	// Y: 바닥 너비에 맞춤
	// ★ 로컬 Bounds 기반 (회전에 의한 월드 AABB 왜곡 방지)
	if (Chunk)
	{
		TargetWidth = GetFloorWidth(Chunk);
	}

	// 주인님 지시사항: 박스의 스케일 크기(두께)는 1 ~ 1.5배로 랜덤 처리
	float TargetHeightScale = FMath::RandRange(1.0f, 1.5f);

	// 2. 통합된 스케일 계산 적용 (Z 스케일은 예외 처리)
	FVector TargetSize(TargetDepth, TargetWidth, 100.f); // Z는 나중에 덮어쓸 것이므로 임의의 값 100.f 전달
	FVector NewScale = CalculateObstacleScale(Obstacle, TargetSize);
	
	// 3. 최종 스케일 적용 (Z만 특수 스케일 적용)
	Obstacle->SetActorScale3D(FVector(
		NewScale.X,  // X: 두께
		NewScale.Y,  // Y: 바닥 폭
		TargetHeightScale // Z: 장애물 두께 스케일
	));

	// 4. 장애물 정보 주입 (Interface)
	FExObstacleInfo Info;
	Info.Type = EExObstacleType::Slide;
	Info.Value = TargetDepth; // X축 길이(두께)
	LastGeneratedInfoValue = TargetDepth;
	ApplyObstacleInfo(Obstacle, Info);
}

// ──────────────────────────────────────────────
// CalculateSpawnPosition: Slide 전용 위치 계산
// Z = Floor.Z + PassHeight (데이터 애셋의 MinSize.Z ~ MaxSize.Z 랜덤값 기준)
// ──────────────────────────────────────────────
FTransform UExObstacleStrategy_Slide::CalculateSpawnPosition_Implementation(
	const UExObstacleDefinition* Def,
	AExFloorChunk* Chunk,
	float SafeStartX)
{
	// 1. 부모 클래스의 기본 스폰 위치 계산 로직 실행 (Pitch 보정 반영)
	FTransform WorldTrans = Super::CalculateSpawnPosition_Implementation(Def, Chunk, SafeStartX);
	if (!Chunk) return WorldTrans;

	// 2. 데이터 애셋에서 읽은 슬라이드 필요 공간 (통과 높이)
	// 주인님 지시사항: 캐릭터 캡슐 정보를 무시하고 Def->MinSize.Z ~ MaxSize.Z를 띄워줄 높이(Z) 기준으로 사용합니다.
	float PassHeight = 130.f;
	if (Def)
	{
		PassHeight = FMath::RandRange(Def->MinSize.Z, Def->MaxSize.Z);
	}

	// 3. 순수 바닥(TrueFloorZ)의 정확한 수직(Z) 위치 도출
	float ChunkStartDist = Chunk->PathDistance - (Chunk->ChunkLength * 0.5f);
	float LocalDist = (SafeStartX + 200.f) - ChunkStartDist;
	
	FTransform PureCurveTrans = Chunk->GetLocalTransformAtDistance(LocalDist);
	
	// Y 오프셋(너비) 반영
	float TargetWidth = GetFloorWidth(Chunk);
	float YOffset = -(TargetWidth * 0.5f);
	FVector RightDirFront = PureCurveTrans.GetRotation().GetRightVector();
	PureCurveTrans.AddToTranslation(RightDirFront * YOffset);

	// 월드 기준 순수 바닥면 위치
	FTransform PureWorldTrans = PureCurveTrans * Chunk->GetActorTransform();
	float TrueFloorZ = PureWorldTrans.GetLocation().Z;

	// 4. 월드 트랜스폼의 수직 방향(UpVector)을 기준으로 Z 오프셋 1차 추가
	FVector UpDir = WorldTrans.GetRotation().GetUpVector();
	WorldTrans.AddToTranslation(UpDir * PassHeight);

	// 5. [절대 검증] 실제 수직 확보 거리(장애물 바닥 - 실제 바닥)가 PassHeight와 일치하는지 확인
	float CurrentObstacleZ = WorldTrans.GetLocation().Z;
	float ExactVerticalClearance = CurrentObstacleZ - TrueFloorZ;

	// 강제 보정 로직
	if (FMath::Abs(ExactVerticalClearance - PassHeight) > 1.0f)
	{
		FVector CorrectedLocation = WorldTrans.GetLocation();
		CorrectedLocation.Z = TrueFloorZ + PassHeight;
		WorldTrans.SetLocation(CorrectedLocation);
		
		UE_LOG(LogTemp, Warning, TEXT("[Slide Strategy] Z Corrected: TrueFloorZ=%.1f, Original Z=%.1f -> Forced Z=%.1f (PassHeight=%.1f)"), 
			TrueFloorZ, CurrentObstacleZ, CorrectedLocation.Z, PassHeight);
	}

	return WorldTrans;
}
