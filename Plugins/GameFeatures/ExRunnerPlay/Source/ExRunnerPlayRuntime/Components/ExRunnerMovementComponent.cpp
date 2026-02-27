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
	// 더 이상 매 프레임 TickComponent를 오버라이드하여 돌리지 않고 내부 함수 바인딩 등에만 사용. 
	// (Tick에서 처리하던 레인 변경 로직은 별도 틱을 켜두거나, 타이머/기타 방식으로 처리해야 하지만 
	// 레인 변경 보간 로직(UpdateLanePosition) 자체는 프레임레이트 의존적이므로 기본 틱은 켜둡니다.)
	PrimaryComponentTick.bCanEverTick = true;
}

// BeginPlay에서 상위 Pawn의 MoverComponent에 파트너 등록을 시도합니다.
void UExRunnerMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	// 1차 시도 (즉시 성공할 수도 있음)
	TryInitializeMover();

	// 만약 초기화에 실패했다면(부착이 안 끝났다면), 가벼운 타이머(0.1초마다)로 재시도합니다.
	if (!TargetPawn)
	{
		GetWorld()->GetTimerManager().SetTimer(InitTimerHandle, this, &UExRunnerMovementComponent::TryInitializeMover, 0.1f, true);
	}
}

void UExRunnerMovementComponent::TryInitializeMover()
{
	if (TargetPawn) return; // 이미 초기화 완료

	// 상위 Pawn 찾기 (1순위: Owner, 2순위: AttachParent)
	APawn* ParentPawn = Cast<APawn>(GetOwner());
	if (!ParentPawn && GetOwner())
	{
		ParentPawn = Cast<APawn>(GetOwner()->GetAttachParentActor());
	}

	// 상위 Pawn의 MoverComponent를 찾아 InputProducer로 등록
	if (ParentPawn)
	{
		UMoverComponent* MoverComp = ParentPawn->FindComponentByClass<UMoverComponent>();
		if (MoverComp)
		{
			TargetPawn = ParentPawn; // 캐싱 완료
			// InputProducers 배열에 자신을 추가하여 Mover가 매 틱마다 ProduceInput을 호출하도록 합니다.
			MoverComp->InputProducers.AddUnique(this);
			UE_LOG(LogExRunnerMovement, Log, TEXT("ExRunnerMovement: 상위 Pawn '%s'의 MoverComponent에 InputProducer로 등록 완료"), *ParentPawn->GetName());
			
			// 성공 시 타이머 안전 종료
			GetWorld()->GetTimerManager().ClearTimer(InitTimerHandle);
		}
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

	// 아직 부모 폰에 안 붙었다면 위치 업데이트 로직 등은 스킵
	if (!TargetPawn) return;

	// 4. 레인 변경 처리 (보간)
	UpdateLanePosition(DeltaTime);
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
