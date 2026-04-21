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

	/** 현재 레인 인덱스 반환 (-1=좌, 0=중앙, 1=우) */
	UFUNCTION(BlueprintPure, Category = "Runner|Lane")
	int32 GetCurrentLaneIndex() const { return CurrentLaneIndex; }

	/**
	 * 레인 보간(이동)이 완료되었는지 여부 반환
	 * CurrentLaneYOffset이 TargetY에 충분히 근접했으면 true
	 */
	UFUNCTION(BlueprintPure, Category = "Runner|Lane")
	bool IsLaneTransitionComplete() const;

	/** AutoRun 모드 활성화 여부 설정 (AutoRun Strategy에서 호출) */
	UFUNCTION(BlueprintCallable, Category = "Runner|Mode")
	void SetAutoRunMode(bool bEnabled);

	/** [추가] 물리 의존 없이 SetActorLocation으로 횡이동(Drift 억제) 할지 여부 설정 */
	UFUNCTION(BlueprintCallable, Category = "Runner|Mode")
	void SetUseDirectLateralMovement(bool bEnable);

	/** AutoRun 전용 레인 변경 콜백 — OnLaneChangeRequested 델리게이트에 바인딩 */
	UFUNCTION()
	void OnLaneChangeRequestedCallback(int32 LaneDirection);

protected:
	/** 제어 대상 폰 (Mover) - Pawn 기반 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Runner")
	TObjectPtr<APawn> TargetPawn;

	/** 데이터센터에서 로드된 러너 설정 (LaneWidth, LaneChangeSpeed, GameMode 변수 등 포함) */
	UPROPERTY(Transient)
	TObjectPtr<class UExRunnerConfig> CachedConfig;

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
	bool bIsAutoRunMode = true; // [수정] 기본값 true로 설정하여 딜레이 시에도 오토런 진행

	/** 물리 이동 대신 위치 강제 보정(SetActorLocation)을 통해 레인 이동을 수행할지 여부 */
	bool bUseDirectLateralMovement = false;

	// 조이스틱 좌우 입력으로 설정된 목표 Yaw 오프셋 (°) — 경로 기준 정면으로부터의 편차
	// NormX(-1~1) × MaxRunnerYawAngle 로 계산되며, Release 시 0.0으로 초기화
	float TargetLookYawOffset = 0.0f;

	// 동적 델리게이트 바인딩 지연 여부 체크
	bool bIsLookInputBound = false;

	/** 동적으로 계산되거나 설정값에서 가져온 현재 레인 간격 캐시 */
	UPROPERTY(Transient)
	float DynamicLaneWidth = 300.0f;

public:
	// 점프 직전에 곡선 구간을 예측하여 향해야 할 Yaw 각도로 캐릭터를 회전시킵니다.
	void ApplyPreJumpRotation();

	// OnLookRequested 델리게이트 콜백 (Dynamic Multicast 바인딩용)
	// Strategy 클래스에서 RemoveDynamic 참조를 위해 public 선언
	UFUNCTION()
	void OnLookRequestedCallback(float AxisValue);

private:
	/** 이동/조향 디버그 정보 시각화 (Cheat 전용 독립 함수) */
	void DrawDebugMovementInfo(const FRotator& TargetRot, const FRotator& TargetControlRot, const FRotator& CurrentControlRot);



};
