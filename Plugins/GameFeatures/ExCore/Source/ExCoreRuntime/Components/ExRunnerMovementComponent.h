#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MoverSimulationTypes.h"
#include "ExRunnerMovementComponent.generated.h"

class ACharacter;
struct FMoverInputCmdContext;
struct FCharacterDefaultInputs;

/**
 * 러너 게임을 위한 무브먼트 제어 컴포넌트
 * - Visual Actor에 부착되어 상위 Mover 캐릭터를 조종
 * - 자동 전진 및 레인 변경 로직 담당
 * - IMoverInputProducerInterface를 구현하여 Mover 시스템과 연동
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class EXCORERUNTIME_API UExRunnerMovementComponent : public UActorComponent, public IMoverInputProducerInterface
{
	GENERATED_BODY()

public:	
	UExRunnerMovementComponent();

protected:
	virtual void BeginPlay() override;

	// IMoverInputProducerInterface 구현
	virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 왼쪽 레인으로 이동 요청 (BP에서 호출) */
	UFUNCTION(BlueprintCallable, Category = "Runner|Movement")
	void MoveLeft();

	/** 오른쪽 레인으로 이동 요청 (BP에서 호출) */
	UFUNCTION(BlueprintCallable, Category = "Runner|Movement")
	void MoveRight();

protected:
	/** 제어 대상 폰 (Mover) - Pawn 기반 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Runner")
	TObjectPtr<APawn> TargetPawn;

	/** 레인 폭 (단위: cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runner|Lane")
	float LaneWidth = 300.0f;

	/** 레인 변경 속도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runner|Lane")
	float LaneChangeSpeed = 10.0f;

private:
	/** 레인 위치 업데이트 (Interp) */
	void UpdateLanePosition(float DeltaTime);

	// 내부 상태 변수
	int32 CurrentLaneIndex = 0;
	float CurrentLaneYOffset = 0.0f;
};
