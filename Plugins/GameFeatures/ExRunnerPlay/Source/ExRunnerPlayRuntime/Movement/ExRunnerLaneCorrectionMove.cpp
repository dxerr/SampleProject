// Copyright ExFrameWork. All Rights Reserved.

#include "ExRunnerLaneCorrectionMove.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoveLibrary/MovementUtilsTypes.h"
#include "../Components/ExRunnerMovementComponent.h"
#include "../GameStates/ExRunnerGameState.h"
#include "../Components/ExPathManager.h"

FLayeredMove_LaneCorrection::FLayeredMove_LaneCorrection()
{
	// 무한 지속 (AutoRun 모드 해제 시 수동으로 종료)
	DurationMs = -1.0f;
	// 기존 이동 속도에 횡방향 보정 속도를 "더하기" 모드로 합산
	MixMode = EMoveMixMode::AdditiveVelocity;
	Priority = 0;
}

bool FLayeredMove_LaneCorrection::GenerateMove(
	const FMoverTickStartData& StartState,
	const FMoverTimeStep& TimeStep,
	const UMoverComponent* MoverComp,
	UMoverBlackboard* SimBlackboard,
	FProposedMove& OutProposedMove)
{
	if (!OwnerMovementComp.IsValid() || !MoverComp) return false;

	const UExRunnerMovementComponent* MovComp = OwnerMovementComp.Get();
	const AActor* OwnerActor = MoverComp->GetOwner();
	if (!OwnerActor) return false;

	// GameState에서 경로 데이터 획득
	UWorld* World = MoverComp->GetWorld();
	if (!World) return false;

	AExRunnerGameState* GS = World->GetGameState<AExRunnerGameState>();
	if (!GS || !GS->PathManager) return false;

	// 현재 경로 위의 정확한 좌표와 우측 직교 벡터 추출
	float PlayerPathDist = GS->RealPlayerPathDistance;
	FVector ExactPathPoint = GS->PathManager->GetPositionAtDistance(PlayerPathDist);
	FRotator PathDir = GS->PathManager->GetDirectionAtDistance(PlayerPathDist);
	FVector ExactPathRight = FRotationMatrix(PathDir).GetScaledAxis(EAxis::Y);

	// 우리가 있어야 할 이상적 레인 위 좌표 (Z축은 현재 위치 보존)
	FVector CurrentLoc = OwnerActor->GetActorLocation();
	float CurrentLaneOffset = MovComp->GetCurrentLaneYOffset();
	FVector IdealLoc = ExactPathPoint + (ExactPathRight * CurrentLaneOffset);
	IdealLoc.Z = CurrentLoc.Z; // 점프 높이는 절대 건드리지 않음

	// 횡방향 이탈 오차 계산 (Z=0 평면에서만)
	FVector LateralError = IdealLoc - CurrentLoc;
	LateralError.Z = 0.0f;

	// 오차가 거의 없으면 보정 속도를 주입하지 않음 (불필요한 연산 방지)
	if (LateralError.SizeSquared() < 1.0f)
	{
		OutProposedMove.LinearVelocity = FVector::ZeroVector;
		return true;
	}

	// 이탈 오차 × 보정 강도 = 초속 단위 보정 속도 벡터
	// Mover가 물리적으로 충돌 검사를 거친 뒤 안전하게 캐릭터를 밀어줍니다
	OutProposedMove.LinearVelocity = LateralError * CorrectionStrength;
	OutProposedMove.MixMode = EMoveMixMode::AdditiveVelocity;

	return true;
}

FLayeredMoveBase* FLayeredMove_LaneCorrection::Clone() const
{
	FLayeredMove_LaneCorrection* CopyPtr = new FLayeredMove_LaneCorrection(*this);
	return CopyPtr;
}

void FLayeredMove_LaneCorrection::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
	// 추가 직렬화 필드가 필요하면 여기에 추가
}

UScriptStruct* FLayeredMove_LaneCorrection::GetScriptStruct() const
{
	return FLayeredMove_LaneCorrection::StaticStruct();
}

FString FLayeredMove_LaneCorrection::ToSimpleString() const
{
	return FString::Printf(TEXT("LaneCorrection (Strength=%.1f)"), CorrectionStrength);
}
