#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MoverSimulationTypes.h"
#include "ExRunnerMovementComponent.generated.h"

class ACharacter;
class UMoverComponent;
struct FMoverInputCmdContext;
struct FCharacterDefaultInputs;

/**
 * 러너 게임을 위한 무브먼트 제어 컴포넌트
 * - Visual Actor(Child Actor)에 부착되어 상위 Mover 캐릭터를 조종
 * - BeginPlay 시 상위 Pawn의 UMoverComponent에 자신을 InputProducer로 등록
 * - 자동 전진 및 레인 변경 로직 담당
 * - IMoverInputProducerInterface를 구현하여 Mover 시스템과 연동
 */
UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
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
	// 레인 변경(UpdateLanePosition) 처리를 위해 Tick을 유지합니다.
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 왼쪽 레인으로 이동 요청 (BP에서 호출) */
	UFUNCTION(BlueprintCallable, Category = "Runner|Movement")
	void MoveLeft();

	/** 오른쪽 레인으로 이동 요청 (BP에서 호출) */
	UFUNCTION(BlueprintCallable, Category = "Runner|Movement")
	void MoveRight();

	/** 현재 러너의 목표 베이스 스피드를 변경합니다. (이때 StatComponent를 통해 UI 트리거) */
	UFUNCTION(BlueprintCallable, Category = "Runner|Movement")
	void SetTargetRunningSpeed(float NewSpeed);

	/** 조이스틱 Look 입력 바인딩 (ExRunnerInputComponent 의 OnLookRequested 델리게이트 연결용) */
	UFUNCTION(BlueprintCallable, Category = "Runner|Input")
	void BindLookInput(class UExRunnerInputComponent* InputComp);

	/** 현재 레인의 목표 좌우 오프셋 반환 */
	UFUNCTION(BlueprintPure, Category = "Runner|Lane")
	float GetCurrentLaneYOffset() const { return CurrentLaneYOffset; }

	/** AutoRun 모드 활성화 여부 설정 (AutoRun Strategy에서 호출) */
	UFUNCTION(BlueprintCallable, Category = "Runner|Mode")
	void SetAutoRunMode(bool bEnabled);

	/** AutoRun 전용 레인 변경 콜백 — OnLaneChangeRequested 델리게이트에 바인딩 */
	UFUNCTION()
	void OnLaneChangeRequestedCallback(int32 LaneDirection);

protected:
	/** 제어 대상 폰 (Mover) - Pawn 기반 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Runner")
	TObjectPtr<APawn> TargetPawn;

	/** 레인 폭 (단위: cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runner|Lane")
	float LaneWidth = 100.0f;

	/** 레인 변경 속도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runner|Lane")
	float LaneChangeSpeed = 10.0f;
	/** GameModeDataSet (MaxRunnerYawAngle, LookInterpSpeed 참조용) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Runner|Look")
	class UExGameModeDataSet* GameModeDataSet;

private:
	/** 레인 위치 업데이트 (Interp) */
	void UpdateLanePosition(float DeltaTime);

	/** 캐릭터 조향(Rotation) 및 스티어링 업데이트 (GameState의 PathManager 연동) */
	UFUNCTION(BlueprintCallable, Category = "Runner|Movement")
	void UpdateCharacterRotation(float DeltaTime);

	/** 상위 Mover Pawn 바인딩 시도 (성공 시 타이머 종료) */

	void TryInitializeMover();

	/** 지연 초기화를 위한 타이머 핸들 */
	FTimerHandle InitTimerHandle;

	/** 중앙 관리 스탯 컴포넌트 캐싱 (속도 갱신 시 UI 전달용) */
	UPROPERTY()
	TWeakObjectPtr<class UExRunnerStatComponent> CachedStatComponent;

	// 내부 상태 변수
	int32 CurrentLaneIndex = 0;
	float CurrentLaneYOffset = 0.0f;
	bool bIsLaneWidthCalculated = false;

	/** AutoRun 모드 플래그 — UpdateLanePosition 보간 연산 활성화 여부 제어 */
	bool bIsAutoRunMode = false;

	// 조이스틱 좌우 입력으로 설정된 목표 Yaw 오프셋 (°) — 경로 기준 정면으로부터의 편차
	// NormX(-1~1) × MaxRunnerYawAngle 로 계산되며, Release 시 0.0으로 초기화
	float TargetLookYawOffset = 0.0f;

	// 동적 델리게이트 바인딩 지연 여부 체크
	bool bIsLookInputBound = false;


public:
	// OnLookRequested 델리게이트 콜백 (Dynamic Multicast 바인딩용)
	// Strategy 클래스에서 RemoveDynamic 참조를 위해 public 선언
	UFUNCTION()
	void OnLookRequestedCallback(float AxisValue);

private:
	/** 이동/조향 디버그 정보 시각화 (Cheat 전용 독립 함수) */
	void DrawDebugMovementInfo(const FRotator& TargetRot, const FRotator& TargetControlRot, const FRotator& CurrentControlRot);



};
