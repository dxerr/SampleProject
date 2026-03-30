// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LayeredMove.h"
#include "ExRunnerLaneCorrectionMove.generated.h"

class UExRunnerMovementComponent;

/**
 * FLayeredMove_LaneCorrection
 * AutoRun 모드 전용 — Mover 물리 파이프라인 안에서 합법적으로 횡방향(Lateral) 보정 속도를 주입하여
 * 공중(Falling) 등 특수 상태에서도 캐릭터가 레인 중앙으로 부드럽게 수렴하도록 합니다.
 *
 * SetActorLocation 강제 호출 없이 Mover의 SyncState 시스템과 충돌 없이 동작하므로
 * Z축 덜덜거림(Jitter/Depenetration Bounce)이 발생하지 않습니다.
 */
USTRUCT()
struct EXRUNNERPLAYRUNTIME_API FLayeredMove_LaneCorrection : public FLayeredMoveBase
{
	GENERATED_BODY()

	FLayeredMove_LaneCorrection();

	// 소유 MovementComponent (이상적 위치 및 레인 오프셋 데이터 참조용)
	UPROPERTY()
	TWeakObjectPtr<UExRunnerMovementComponent> OwnerMovementComp;

	/** 보정 강도 — 큰 값일수록 빠르게 레인 중앙으로 수렴 (초당 배율) */
	UPROPERTY()
	float CorrectionStrength = 15.0f;

	// FLayeredMoveBase 인터페이스
	virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;
	virtual FLayeredMoveBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;
};

template<>
struct TStructOpsTypeTraits<FLayeredMove_LaneCorrection> : public TStructOpsTypeTraitsBase2<FLayeredMove_LaneCorrection>
{
	enum { WithCopy = true };
};
