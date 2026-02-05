#include "ExRunnerMovementComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "../GameModes/ExRunnerGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MotionWarpingComponent.h" 
#include "MoverDataModelTypes.h"

UExRunnerMovementComponent::UExRunnerMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

// BeginPlay에서는 아무것도 하지 않고 Tick에서 지연 초기화 수행
void UExRunnerMovementComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UExRunnerMovementComponent::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	// Mover 시스템에 전달할 입력 데이터를 찾거나 생성합니다.
	FCharacterDefaultInputs* Inputs = InputCmdResult.InputCollection.FindMutableDataByType<FCharacterDefaultInputs>();
	if (Inputs)
	{
		// 1. 전진 입력 (Runner Game 특성상 항상 전진)
		// Owner(혹은 TargetPawn)의 Forward Vector를 사용
		FVector ForwardDir = FVector::ForwardVector;
		
		if (TargetPawn)
		{
			ForwardDir = TargetPawn->GetActorForwardVector();
		}
		else if (AActor* Owner = GetOwner())
		{
			ForwardDir = Owner->GetActorForwardVector();
		}

		// DirectionalIntent로 이동 입력 설정 (크기 1.0)
		Inputs->SetMoveInput(EMoveInputType::DirectionalIntent, ForwardDir);
	}
}

void UExRunnerMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// TargetPawn이 없으면 찾기 시도 (Lazy Init)
	// Visual Actor가 스폰된 직후에는 아직 Mover에 Attach되지 않았을 수 있기 때문
	if (!TargetPawn)
	{
		AActor* Owner = GetOwner();
		if (Owner)
		{
			// 1. Attach Parent 확인 (Pawn 캐스팅)
			TargetPawn = Cast<APawn>(Owner->GetAttachParentActor());
			
			// 2. Owner 자체 확인
			if (!TargetPawn)
			{
				TargetPawn = Cast<APawn>(Owner);
			}

			if (TargetPawn)
			{
				UE_LOG(LogTemp, Log, TEXT("ExRunnerMovementComponent: Found Target Pawn: %s"), *TargetPawn->GetName());
			}
		}

		// 여전히 없으면 이번 틱은 스킵
		if (!TargetPawn)
		{
			// 너무 빈번한 로그 방지 (2초마다)
			static float LogTimer = 0.0f;
			LogTimer += DeltaTime;
			if (LogTimer > 2.0f)
			{
				LogTimer = 0.0f;
				UE_LOG(LogTemp, Warning, TEXT("[RunnerDebug] Waiting for TargetPawn... Owner: %s (AttachParent is NULL?)"), 
					GetOwner() ? *GetOwner()->GetName() : TEXT("None"));
			}
			return;
		}
	}

	// 1. 게임 모드에서 속도 가져오기
	float RunnerSpeed = 600.f;
	AExRunnerGameMode* GM = Cast<AExRunnerGameMode>(UGameplayStatics::GetGameMode(this));
	if (GM)
	{
		RunnerSpeed = GM->GetCurrentGameSpeed();
	}

	// 2. 캐릭터 MaxWalkSpeed 동기화 (CMC가 있는 경우)
	bool bHasCMC = false;
	if (TargetPawn)
	{
		// 1차 시도: 표준 인터페이스
		UPawnMovementComponent* MovementComp = TargetPawn->GetMovementComponent();
		
		// 1. CharacterMovementComponent 확인
		UCharacterMovementComponent* CMC = Cast<UCharacterMovementComponent>(MovementComp);
		
		// 2차 시도: 컴포넌트 직접 검색
		if (!CMC) CMC = TargetPawn->FindComponentByClass<UCharacterMovementComponent>();

		if (CMC)
		{
			CMC->MaxWalkSpeed = RunnerSpeed;
			bHasCMC = true;
		}
		else
		{
			// CMC를 못 찾으면 가끔 경고
			static float CMCCheckTimer = 0.0f;
			CMCCheckTimer += DeltaTime;
			if (CMCCheckTimer > 3.0f)
			{
				CMCCheckTimer = 0.0f;
				
				FString PawnClassName = TargetPawn->GetClass()->GetName();
				// 보유 컴포넌트 리스트 덤프 (다시 확인용)
				UE_LOG(LogTemp, Warning, TEXT("[RunnerDebug] Failed to find CMC on %s! Class: %s"), *TargetPawn->GetName(), *PawnClassName);
			}
		}
	}

	// 3. 강제 전진 (Forward Vector)
	// [Treadmill Mode] 플레이어는 X축 고정, 월드(Floor)가 뒤로 이동함.
	// 따라서 전진 이동 로직은 비활성화합니다.
	
	/*
	if (bHasCMC)
	{
		TargetPawn->AddMovementInput(TargetPawn->GetActorForwardVector());
	}
	else
	{
		// 직접 이동 처리 (Sweep=true로 충돌 감지)
		FVector DeltaMove = TargetPawn->GetActorForwardVector() * RunnerSpeed * DeltaTime;
		TargetPawn->AddActorWorldOffset(DeltaMove, true);
	}
	*/

	// 4. 레인 변경 처리
	UpdateLanePosition(DeltaTime);

	// [Debug] 상태 모니터링 코드 제거(불필요)
}

void UExRunnerMovementComponent::MoveLeft()
{
	if (CurrentLaneIndex > -1)
	{
		CurrentLaneIndex--;
	}
}

void UExRunnerMovementComponent::MoveRight()
{
	if (CurrentLaneIndex < 1)
	{
		CurrentLaneIndex++;
	}
}

void UExRunnerMovementComponent::UpdateLanePosition(float DeltaTime)
{
	if (!TargetPawn) return;
	
	// 목표 오프셋 계산
	float TargetY = CurrentLaneIndex * LaneWidth;
	
	// 현재 오프셋 보간
	float OldY = CurrentLaneYOffset;
	CurrentLaneYOffset = FMath::FInterpTo(CurrentLaneYOffset, TargetY, DeltaTime, LaneChangeSpeed);
	
	float DeltaY = CurrentLaneYOffset - OldY;
	
	if (!FMath::IsNearlyZero(DeltaY))
	{
		// 횡이동: RightVector * DeltaY 만큼 이동
		// Mover를 직접 이동시킴 (Sweep=true 충돌 체크)
		FVector RightDir = TargetPawn->GetActorRightVector();
		TargetPawn->AddActorWorldOffset(RightDir * DeltaY, true);
	}
}

void UExRunnerMovementComponent::SetInteractionTarget(USceneComponent* TargetComponent, FName WarpTargetName)
{
	if (!TargetComponent) return;

	CurrentInteractionTarget = TargetComponent;
	CurrentWarpTargetName = WarpTargetName;
	
	UE_LOG(LogTemp, Log, TEXT("ExRunnerMovement: Interaction Target Set to %s (Warp: %s)"), 
		*TargetComponent->GetName(), 
		*WarpTargetName.ToString());

	// Motion Warping Target 등록 (bFollowComponent=true로 설정하여 움직이는 장애물 추적)
	if (!MotionWarpingComp.IsValid())
	{
		if (TargetPawn)
		{
			MotionWarpingComp = TargetPawn->FindComponentByClass<UMotionWarpingComponent>();
		}
		else if (AActor* Owner = GetOwner())
		{
			// 혹시 TargetPawn이 아직 세팅 안됐을 경우 Owner 검색
			MotionWarpingComp = Owner->FindComponentByClass<UMotionWarpingComponent>();
		}
	}

	if (MotionWarpingComp.IsValid())
	{
		// 중요: bFollowComponent = true
		// 이렇게 하면 MotionWarpingComponent가 Tick마다 이 컴포넌트의 위치를 추적하여 Warp Target을 자동 갱신함.
		// Treadmill System으로 장애물이 뒤로 밀려나도, Warp 위치는 Obstacle 기준이므로 정확히 동기화됨.
		MotionWarpingComp->AddOrUpdateWarpTargetFromComponent(
			WarpTargetName, 
			TargetComponent, 
			NAME_None, 
			true,
			EWarpTargetLocationOffsetDirection::TargetsForwardVector,
			FVector::ZeroVector,
			FRotator::ZeroRotator
		);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ExRunnerMovement] Failed to find MotionWarpingComponent on Owner!"));
	}

	// [Fix: Treadmill Pause Strategy]
	// 등반 중에는 트레드밀을 멈춰서 동기화 문제와 조기 종료(Premature Overlap End) 문제를 원천 차단함.
	if (UWorld* World = GetWorld())
	{
		if (AExRunnerGameMode* GM = Cast<AExRunnerGameMode>(World->GetAuthGameMode()))
		{
			GM->SetTreadmillPaused(true);
		}
	}
}

void UExRunnerMovementComponent::ClearInteractionTarget()
{
	if (MotionWarpingComp.IsValid() && !CurrentWarpTargetName.IsNone())
	{
		MotionWarpingComp->RemoveWarpTarget(CurrentWarpTargetName);
	}

	CurrentInteractionTarget = nullptr;
	CurrentWarpTargetName = NAME_None;

	// [Fix: Treadmill Pause Strategy]
	// 등반 종료 시 트레드밀 재개
	if (UWorld* World = GetWorld())
	{
		if (AExRunnerGameMode* GM = Cast<AExRunnerGameMode>(World->GetAuthGameMode()))
		{
			GM->SetTreadmillPaused(false);
		}
	}
}
