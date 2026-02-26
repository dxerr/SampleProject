#include "ExRunnerMovementComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "../GameModes/ExRunnerGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MotionWarpingComponent.h" 
#include "MoverDataModelTypes.h"
#include "MoverComponent.h"

// 디버깅용 로그 카테고리 정의
DEFINE_LOG_CATEGORY_STATIC(LogExRunnerMovement, Log, All);

UExRunnerMovementComponent::UExRunnerMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

// BeginPlay에서 상위 Pawn의 MoverComponent에 자신을 InputProducer로 등록합니다.
void UExRunnerMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	// 상위 Pawn 찾기 (1순위: Owner, 2순위: AttachParent)
	APawn* ParentPawn = Cast<APawn>(GetOwner());
	if (!ParentPawn)
	{
		ParentPawn = Cast<APawn>(GetOwner()->GetAttachParentActor());
	}

	// 상위 Pawn의 MoverComponent를 찾아 InputProducer로 등록
	if (ParentPawn)
	{
		UMoverComponent* MoverComp = ParentPawn->FindComponentByClass<UMoverComponent>();
		if (MoverComp)
		{
			// InputProducers 배열에 자신을 추가하여 Mover가 매 틱마다 ProduceInput을 호출하도록 합니다.
			MoverComp->InputProducers.AddUnique(this);
			UE_LOG(LogExRunnerMovement, Log, TEXT("ExRunnerMovement: 상위 Pawn '%s'의 MoverComponent에 InputProducer로 등록 완료"), *ParentPawn->GetName());
		}
		else
		{
			UE_LOG(LogExRunnerMovement, Warning, TEXT("ExRunnerMovement: 상위 Pawn '%s'에서 MoverComponent를 찾을 수 없습니다."), *ParentPawn->GetName());
		}
	}
	else
	{
		UE_LOG(LogExRunnerMovement, Warning, TEXT("ExRunnerMovement: 상위 Pawn을 찾을 수 없습니다. ProduceInput이 호출되지 않습니다."));
	}
}

void UExRunnerMovementComponent::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	// Mover 시스템에 전달할 입력 데이터를 찾거나 생성합니다.
	FCharacterDefaultInputs* Inputs = InputCmdResult.InputCollection.FindMutableDataByType<FCharacterDefaultInputs>();
	if (Inputs)
	{
		// 1. 순수 전진 입력 (PlayerController의 전방 축 기준)
		// 곡선 구간이나 카메라 회전 시에도 조작 시점에 '앞(X)'으로 간주되는 방향으로 전진
		FVector ForwardDir = FVector::ForwardVector;
		
		if (TargetPawn)
		{
			if (AController* PawnController = TargetPawn->GetController())
			{
				const FRotator Rotation = PawnController->GetControlRotation();
				const FRotator YawRotation(0, Rotation.Yaw, 0);
				ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
			}
		}

		// DirectionalIntent로 이동 입력 설정 (크기 1.0)
		Inputs->SetMoveInput(EMoveInputType::DirectionalIntent, ForwardDir);

		// [Fix] Mover 시스템이 캐릭터의 모델(Mesh) 방향도 컨트롤러가 바라보는 방향(경로 접선)으로 맞춰 회전시킬 수 있도록 OrientationIntent를 주입합니다.
		Inputs->OrientationIntent = ForwardDir;
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
				UE_LOG(LogExRunnerMovement, Log, TEXT("ExRunnerMovementComponent: Found Target Pawn: %s"), *TargetPawn->GetName());
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
				UE_LOG(LogExRunnerMovement, Warning, TEXT("[RunnerDebug] Waiting for TargetPawn... Owner: %s (AttachParent is NULL?)"), 
					GetOwner() ? *GetOwner()->GetName() : TEXT("None"));
			}
			return;  // TargetPawn 없으면 스킵
		}
	}

	// [NOTE] 캐릭터 MaxWalkSpeed는 ABP 또는 상태 시스템에서 자체 관리됨
	// 트레드밀 속도와 분리되어 별도 설정 불필요

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
	
	// 목표 레인 오프셋 계산 (가운데: 0, 왼쪽: -LaneWidth, 오른쪽: +LaneWidth)
	float TargetY = CurrentLaneIndex * LaneWidth;
	
	// 현재 오프셋 보간
	float OldY = CurrentLaneYOffset;
	CurrentLaneYOffset = FMath::FInterpTo(CurrentLaneYOffset, TargetY, DeltaTime, LaneChangeSpeed);
	
	float DeltaY = CurrentLaneYOffset - OldY;
	
	if (!FMath::IsNearlyZero(DeltaY))
	{
		// 횡이동: 캐릭터의 로컬 우측 벡터 기준 DeltaY 만큼 이동
		FVector RightDir = TargetPawn->GetActorRightVector();
		FVector DeltaMove = RightDir * DeltaY;
		
		FHitResult SweepHit;
		TargetPawn->AddActorWorldOffset(DeltaMove, true, &SweepHit);
		
		if (SweepHit.bBlockingHit)
		{
			// 레인 이동 중 충돌 시 로그 (옵션)
			// UE_LOG(LogExRunnerMovement, Warning, TEXT("[LaneMove] Blocked by %s"), *SweepHit.GetActor()->GetName());
		}
	}
}
