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
	// [변경] World X가 아닌 Path Distance 기반 계산
	// SafeStartX 인자는 이제 SafeStartDist(경로 누적 거리)를 의미함
	// PathDistance는 청크의 Center Distance. StartDist는 HalfLength를 빼야 함
	const float ChunkStartDist = Chunk->PathDistance - (ChunkLength * 0.5f);
	
	// 로컬 거리 계산
	// SafeStartX(이전 장애물 끝 거리) + Buffer - 청크 시작 거리
	float LocalDist = (SafeStartX + 200.f) - ChunkStartDist;

	// 범위 체크 (청크 길이 초과 시?)
	// 초과해도 GetLocalTransformAtDistance에서 Clamp 하거나 처리함.

	// 1. 청크 로컬 트랜스폼 계산 (커브 포함)
	FTransform CurveTrans = Chunk->GetLocalTransformAtDistance(LocalDist);

	// 2. Y 오프셋 적용 (바닥 너비 관련)
	// 장애물 Pivot이 Edge에 있다고 가정하고 오프셋 적용
	// ★ 로컬 Bounds 기반 폭 사용 (회전에 의한 월드 AABB 왜곡 방지)
	float TargetWidth = GetFloorWidth(Chunk);
	
	// Curve Center에서 왼쪽으로 Width/2 만큼 이동 (Edge Pivot 보정)
	float YOffset = -(TargetWidth * 0.5f);
	
	// 3. 커브 로컬 변환에 오프셋 적용
	// CurveTrans.GetLocation()은 (X, 0, Z) 형태 (Pitch/Yaw 회전 포함)
	// 로컬 Y축으로 이동
	CurveTrans.AddToTranslation(FVector(0.f, YOffset, 0.f));

	// ★ 중요 (Fix): 장애물이 직선 형태이므로, 곡선 위에서는 단순 접선(Tangent) 기준 기울기보다 
	// 장애물의 시작점(Pivot)과 끝점(End)이 닿는 실제 바닥 좌푯값을 기반으로 한 직접적인 기울기(Pitch)가 
	// 파고들거나 뜨는 현상 없이 평행을 유지하는 데 가장 정확합니다.
	if (Chunk && !FMath::IsNearlyZero(Chunk->CachedHeightOffset) && !FMath::IsNearlyZero(Chunk->CachedCurveAngle))
	{
		float ObsLen = Def ? Def->MaxSize.X : 500.f; // 장애물의 길이(X축 스케일 및 바운드 기준)
		if (ObsLen < 10.f) ObsLen = 100.f; // 최소 길이 보장

		// 1. 앞쪽 기준점 좌표 파악 (YOffset 적용 완료된 CurveTrans 위치)
		FVector FrontPos = CurveTrans.GetLocation();
		
		// 2. 뒤쪽 끝점의 궤도 반환값 계산
		float RearLocalDist = LocalDist + ObsLen;
		FTransform RearTrans = Chunk->GetLocalTransformAtDistance(RearLocalDist);
		RearTrans.AddToTranslation(FVector(0.f, YOffset, 0.f)); // 동일하게 측면 오프셋 적용
		FVector RearPos = RearTrans.GetLocation();

		// 3. 앞점과 뒷점을 관통하는 기하학적 직선 벡터 도출 (현, Chord)
		FVector DirVec = RearPos - FrontPos;

		// 4. 장애물의 회전(Pitch, Yaw)을 이 직선 벡터에 완전히 일치되게 덮어씌움
		FRotator NewLocalRot = CurveTrans.GetRotation().Rotator();
		float Size2D = DirVec.Size2D();
		if (Size2D > KINDA_SMALL_NUMBER)
		{
			// 바닥에 완벽히 밀착되는 Pitch 재계산
			NewLocalRot.Pitch = FMath::RadiansToDegrees(FMath::Atan2(DirVec.Z, Size2D));
			
			// 긴 장애물이 커브 바깥으로 튀어나가지 않도록 방향(Yaw)도 현(Chord) 각도로 보정
			NewLocalRot.Yaw = FMath::RadiansToDegrees(FMath::Atan2(DirVec.Y, DirVec.X));
		}
		CurveTrans.SetRotation(NewLocalRot.Quaternion());
	}
	
	// 4. World 변환 (Chunk Actor Transform 적용)
	FTransform WorldTrans = CurveTrans * Chunk->GetActorTransform();

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

