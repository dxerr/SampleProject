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
class EXRUNNERPLAYRUNTIME_API UExRunnerMovementComponent : public UActorComponent, public IMoverInputProducerInterface
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

	// --- Climb/Obstacle Interaction Sync ---
public:
	/** 
	 * 상호작용(Climb/Vault)할 타겟 설정. 
	 * 이를 호출하면 매 틱마다 MotionWarping Target을 강제로 갱신하여 
	 * World Shift로 밀려나는 오브젝트를 정확히 추적하게 만듭니다.
	 * @param TargetComponent : 잡을 지점 (SceneComponent 등)
	 * @param WarpTargetName : 몽타주에 정의된 WarpName (예: "ClimbPoint")
	 */
	UFUNCTION(BlueprintCallable, Category = "Runner|Interaction")
	void SetInteractionTarget(USceneComponent* TargetComponent, FName WarpTargetName = FName("ClimbPoint"));

	/** 상호작용 종료 (Warping 갱신 중단) */
	UFUNCTION(BlueprintCallable, Category = "Runner|Interaction")
	void ClearInteractionTarget();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Runner|Interaction")
	TObjectPtr<USceneComponent> CurrentInteractionTarget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Runner|Interaction")
	FName CurrentWarpTargetName;

private:
	/** Motion Warping Component Cache */
	TWeakObjectPtr<class UMotionWarpingComponent> MotionWarpingComp;
};
