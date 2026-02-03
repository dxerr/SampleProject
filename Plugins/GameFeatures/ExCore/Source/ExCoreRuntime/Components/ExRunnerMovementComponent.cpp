#include "ExRunnerMovementComponent.h"
#include "ExChunkSpawner.h" 
#include "GameFramework/PawnMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "../GameModes/ExCoreGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MoverDataModelTypes.h" // FCharacterDefaultInputs

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

		// 2. 점프 등 다른 입력 처리 (필요시 추가)
		// 예: bool bJumpPressed = ...;
		// Inputs->bIsJumpPressed = bJumpPressed;
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
	AExCoreGameMode* GM = Cast<AExCoreGameMode>(UGameplayStatics::GetGameMode(this));
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
	// CMC가 있으면 AddMovementInput (관성/충돌 처리 위임)
	// 없으면 직접 이동 (강제 전진)
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

	// 4. 레인 변경 처리
	UpdateLanePosition(DeltaTime);

	// 5. World Shift (Treadmill V2)
	// 캐릭터가 이동한 만큼 세상(바닥)을 반대로 밀고, 캐릭터를 원점으로 복귀
	if (TargetPawn)
	{
		// 5.1. 현재 위치 파악 (X축 이동량)
		FVector CurrentLoc = TargetPawn->GetActorLocation();
		float DeltaX = CurrentLoc.X; 

		// 특정 임계치(예: 100단위) 이상 이동했을 때만 모아서 처리할 수도 있지만,
		// 매 프레임 처리해야 가장 부드러움 (Jittering 방지)
		if (FMath::Abs(DeltaX) > 1.0f) // KINDA_SMALL_NUMBER 대신 1.0f 정도면 충분
		{
			// 5.2. Spawner에게 세상 밀기 요청
			// GM 변수가 상단에 선언되어 있으나(91행), 안전하게 다시 가져오거나 이름 변경하여 사용
			if (AExCoreGameMode* CurrentGM = Cast<AExCoreGameMode>(UGameplayStatics::GetGameMode(this)))
			{
				// GameMode > ChunkSpawner 접근 권한 문제 해결 필요 (Getter 추가 or Friend)
				// 일단 FindComponent로 접근 (임시)
				if (UExChunkSpawner* Spawner = CurrentGM->FindComponentByClass<UExChunkSpawner>())
				{
					Spawner->ShiftWorld(-DeltaX);
				}
			}

			// 5.3. 캐릭터 원위치 (X=0)
			CurrentLoc.X = 0.f;
			TargetPawn->SetActorLocation(CurrentLoc);
		}
	}
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
