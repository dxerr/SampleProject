#include "ExRunnerMovementComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "../GameModes/ExRunnerGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MotionWarpingComponent.h" 
#include "MoverDataModelTypes.h"
#include "MoverComponent.h"
#include "ExRunnerStatComponent.h"
#include "../GameStates/ExRunnerGameState.h"
#include "../Components/ExPathManager.h"
#include "../Data/ExCurveConfig.h"
#include "GameFramework/Character.h"
#include "DrawDebugHelpers.h"
#include "ExRunnerInputComponent.h"
#include "Util/Actor/ExActorUtil.h"
#include "../Actors/ExFloorChunk.h"
#include "Data/Modes/ExGameModeDataSet.h"
#include "ExDebugStateSubsystem.h"
#include "ExGameplayTags.h"

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

	// ExActorUtil을 사용하여 Owner → AttachParent 순으로 Pawn 탐색
	APawn* ParentPawn = UExActorUtil::FindOwnerPawn(this);

	// 상위 Pawn의 MoverComponent를 찾아 InputProducer로 등록
	if (ParentPawn)
	{
		UMoverComponent* MoverComp = ParentPawn->FindComponentByClass<UMoverComponent>();
		if (MoverComp)
		{
			TargetPawn = ParentPawn; // 캐싱 완료
			
			// UI 갱신을 위해 스탯 컴포넌트도 함께 캐싱 (부모 폰에 부착되어 있다고 가정)
			CachedStatComponent = ParentPawn->FindComponentByClass<UExRunnerStatComponent>();
			
			// InputProducers 배열에 자신을 추가하여 Mover가 매 틱마다 ProduceInput을 호출하도록 합니다.
			MoverComp->InputProducers.AddUnique(this);
			
			// Mover 시뮬레이션이 특정 모드에 고착되거나 대기 상태일 수 있으므로 Walking 모드 강제 진입을 요청합니다.
			MoverComp->QueueNextMode(DefaultModeNames::Walking);

			// ★ 핵심: 컨테이너 폰에 상주하는 InputComponent가 나(Movement)를 대기 중일 수 있으므로
			// 초기화(부착) 완료 시점에 찾아가서 자신을 등록하여 Strategy 바인딩을 즉시 촉발(IoC)합니다.
			if (UExRunnerInputComponent* InputComp = TargetPawn->FindComponentByClass<UExRunnerInputComponent>())
			{
				InputComp->RegisterMovementComponent(this);
				bIsLookInputBound = true; // 플래그 갱신
			}

			// 동적으로 월드에 스폰된 바닥(ExFloorChunk)의 Y축 넓이를 기반으로 레인 폭 계산 (3등분)
			TArray<AActor*> FloorChunks;
			UGameplayStatics::GetAllActorsOfClass(this, AExFloorChunk::StaticClass(), FloorChunks);
			if (FloorChunks.Num() > 0)
			{
				if (AExFloorChunk* Chunk = Cast<AExFloorChunk>(FloorChunks[0]))
				{
					float TotalWidth = Chunk->GetFloorBounds().GetSize().Y;
					LaneWidth = TotalWidth / 3.0f;
					bIsLaneWidthCalculated = true;
					UE_LOG(LogExRunnerMovement, Log, TEXT("[ExRunnerMovement] LaneWidth dynamically calculated: %.1f (from TotalWidth: %.1f)"), LaneWidth, TotalWidth);
				}
			}
			else
			{
				// 만약 타이밍 이슈로 아직 청크가 스폰되지 않았다면, Blueprint에서 설정된 기본값을 사용하거나 다음 Tick에서 시도합니다.
				UE_LOG(LogExRunnerMovement, Warning, TEXT("[ExRunnerMovement] No ExFloorChunk found in world during initialization. Falling back to default LaneWidth: %.1f"), LaneWidth);
			}
			
			// 성공 시 타이머 안전 종료
			GetWorld()->GetTimerManager().ClearTimer(InitTimerHandle);
		}
	}
}

void UExRunnerMovementComponent::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	FCharacterDefaultInputs& Inputs = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();
	
	// [개선] 컨트롤러 회전 지연과 차선 보간 문제를 해결하기 위해 경로 매니저 활용
	FVector ForwardDir = FVector::ForwardVector;
	AExRunnerGameState* GS = GetWorld()->GetGameState<AExRunnerGameState>();
	if (GS && GS->PathManager && TargetPawn)
	{
		if (bIsAutoRunMode)
		{
			// Pure Pursuit (경유점 추적): 현재 속도에 기반해 일정 거리(LookAhead) 앞을 목표로 잡습니다.
			float Speed = FMath::Max(TargetPawn->GetVelocity().Size(), 600.f);
			float LookAheadDist = FMath::Clamp(Speed * 0.2f, 200.f, 1000.f); // 약 0.2초 앞 (최소 2m ~ 최대 10m)
			float TargetDist = GS->RealPlayerPathDistance + LookAheadDist;

			// 목표 지점의 경로 위 좌표 및 우측 벡터
			FVector PathPoint = GS->PathManager->GetPositionAtDistance(TargetDist);
			FVector PathRight = FRotationMatrix(GS->PathManager->GetDirectionAtDistance(TargetDist)).GetScaledAxis(EAxis::Y);

			// 최종 타겟 월드 위치 (경로 + 부드럽게 보간된 현재 레인 위치 오프셋)
			FVector TargetPos = PathPoint + (PathRight * CurrentLaneYOffset);
			TargetPos.Z = TargetPawn->GetActorLocation().Z; // Z축 변형 차단



			// ★ 디버깅: 곡선 및 레인 추적(Pure Pursuit) 타겟 시각화
			UExDebugStateSubsystem* DS = GetWorld()->GetGameInstance()->GetSubsystem<UExDebugStateSubsystem>();
			if (DS && DS->IsCheatEnabled(TAG_Ex_Debug_Movement))
			{
				DrawDebugSphere(GetWorld(), TargetPos, 50.0f, 12, FColor::Red, false, -1.f, 0, 2.0f);
				DrawDebugLine(GetWorld(), TargetPawn->GetActorLocation(), TargetPos, FColor::Yellow, false, -1.f, 0, 2.0f);
			}

			// 캐릭터 위치에서 타겟 위치를 향하는 방향 벡터 도출
			FVector BaseDir = (TargetPos - TargetPawn->GetActorLocation()).GetSafeNormal();

			// [점프 공중 체공 중 곡선 방향 유지]
			// 공중에 떠 있을 때(Falling)는 앞서 계산된 타겟 방향에 추가로 예측된 곡선 방향으로 
			// 지속적으로 각도를 꺾어주어야 Mover가 방향키/바라보는 방향을 유지하며 날아갑니다.
			bool bIsFalling = false;
			if (UMoverComponent* MoverComp = TargetPawn->FindComponentByClass<UMoverComponent>())
			{
				bIsFalling = (MoverComp->GetMovementModeName() == FName("Falling"));
			}

			if (bIsFalling && GameModeDataSet)
			{
				float LookAheadLimit = GS->RealPlayerPathDistance + 600.0f;
				FRotator CurrentRot = GS->PathManager->GetDirectionAtDistance(GS->RealPlayerPathDistance);
				FRotator FutureRot = GS->PathManager->GetDirectionAtDistance(LookAheadLimit);
				
				float DeltaYaw = FRotator::NormalizeAxis(FutureRot.Yaw - CurrentRot.Yaw);
				float Weight = GameModeDataSet->JumpYawPredictionWeight;
				
				BaseDir = BaseDir.RotateAngleAxis(DeltaYaw * Weight, FVector::UpVector);
			}

			ForwardDir = BaseDir;
		}
		else
		{
			// 수동 모드의 경우 기존처럼 바로 발밑/가까운 경로의 방향을 바라봄
			float PlayerPathDist = GS->RealPlayerPathDistance;
			FRotator PathRot = GS->PathManager->GetDirectionAtDistance(PlayerPathDist);
			ForwardDir = PathRot.Vector();
		}
	}

	// 1. Mover의 물리적 방향과 모델 지향 방향을 결정하는 핵심 벡터 도출
	FVector GoalDirection = ForwardDir;

	// 임시 디버그: 현재 상태를 화면 좌상단에 실시간 출력
	if (GEngine && TargetPawn && TargetPawn->HasLocalNetOwner())
	{
		GEngine->AddOnScreenDebugMessage(88, 0.0f, bIsAutoRunMode ? FColor::Green : FColor::Red, 
			FString::Printf(TEXT("[ProduceInput] AutoRun: %s | LaneIdx: %d | OffsetY: %.1f"), 
			bIsAutoRunMode ? TEXT("ON") : TEXT("OFF"), CurrentLaneIndex, CurrentLaneYOffset));
	}
	
	// Manual 모드일 때만 조이스틱 방향 각도 오프셋 적용
	if (!bIsAutoRunMode && !FMath::IsNearlyZero(TargetLookYawOffset, 0.1f))
	{
		GoalDirection = ForwardDir.RotateAngleAxis(TargetLookYawOffset, FVector::UpVector);
	}

	// 2. 이 최종 타겟 방향을 Mover의 물리적 이동 방향성(DirectionalInput)으로 지정합니다.
	FVector MergedInput = GoalDirection;
	MergedInput.Z = 0.0f; // 평면 이동 유지
	
	if (MergedInput.SizeSquared() > 1.0f)
	{
		MergedInput.Normalize();
	}

	UMoverDataModelBlueprintLibrary::SetDirectionalInput(Inputs, MergedInput);

	// 3. 캐릭터의 고개가 쳐다볼 방향(OrientationIntent)에도 똑같이 타겟 방향을 꽂습니다.
	// 이제 Mover 내부 시스템이 자기 자신의 보간(TurnGenerator 등)을 거쳐 부드럽게 캐릭터 캡슐을 회전시킵니다!
	if (!MergedInput.IsNearlyZero())
	{
		Inputs.OrientationIntent = MergedInput.GetSafeNormal();
	}
}

void UExRunnerMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 아직 부모 폰에 안 붙었다면 위치 업데이트 로직 등은 스킵
	if (!TargetPawn) return;

	// 순서 보장: 기본 Mover Input Producer가 빈 가상 조이스틱 값(0,0)을 덮어쓰지 못하도록 이 컴포넌트를 항상 마지막에 위치시킴
	UMoverComponent* MoverComp = TargetPawn->FindComponentByClass<UMoverComponent>();
	if (MoverComp && MoverComp->InputProducers.Num() > 0 && MoverComp->InputProducers.Last() != this)
	{
		MoverComp->InputProducers.RemoveSingle(this);
		MoverComp->InputProducers.Add(this);
	}

	// 속도 수집은 ExRunnerStatComponent의 자체 타이머(StatPollInterval)가 담당합니다.

	if (TargetPawn && !bIsLookInputBound)
	{
		if (UExRunnerInputComponent* InputComp = TargetPawn->FindComponentByClass<UExRunnerInputComponent>())
		{
			BindLookInput(InputComp);
			bIsLookInputBound = true;
		}
	}

	// 1. 캐릭터 조향 업데이트 (경로 추적)
	UpdateCharacterRotation(DeltaTime);

	// 2. 레인 변경 처리 (보간)
	UpdateLanePosition(DeltaTime);
}

bool UExRunnerMovementComponent::IsLaneTransitionComplete() const
{
	// TargetY: 목표 레인 오프셋 (CurrentLaneIndex * LaneWidth)
	const float TargetY = CurrentLaneIndex * LaneWidth;
	// 오차 5cm 이내면 이동 완료로 판정
	return FMath::IsNearlyEqual(CurrentLaneYOffset, TargetY, 5.0f);
}

void UExRunnerMovementComponent::MoveLeft()
{
	CurrentLaneIndex = FMath::Clamp(CurrentLaneIndex - 1, -1, 1);
}

void UExRunnerMovementComponent::MoveRight()
{
	CurrentLaneIndex = FMath::Clamp(CurrentLaneIndex + 1, -1, 1);
}

void UExRunnerMovementComponent::SetTargetRunningSpeed(float NewSpeed)
{
	if (CachedStatComponent.IsValid())
	{
		CachedStatComponent->SetCurrentRunningSpeed(NewSpeed);
	}
}

void UExRunnerMovementComponent::BindLookInput(UExRunnerInputComponent* InputComp)
{
	if (!InputComp) return;

	// 기존 바인딩이 있다면 해제 (중복 바인딩 방지)
	InputComp->OnLookRequested.RemoveDynamic(this, &UExRunnerMovementComponent::OnLookRequestedCallback);

	// OnLookRequested 델리게이트 바인딩:
	// NormX(-1~1)를 수신하여 TargetLookYawOffset(°)으로 변환하여 저장합니다.
	InputComp->OnLookRequested.AddDynamic(this, &UExRunnerMovementComponent::OnLookRequestedCallback);
}

void UExRunnerMovementComponent::OnLookRequestedCallback(float AxisValue)
{
	// [수정] GameModeDataSet이 에디터(BP)에서 할당되지 않았을 경우를 대비한 안전 장치.
	// 할당되어 있다면 그 값(0 포함)을 온전히 따르고, 아예 Null이라면 기본값 45도를 사용합니다.
	// 필수 애셋 누락 시 에디터 크래시(check)를 발생시켜 즉시 인지하고 수정하도록 강제합니다. (가이드라인 1.7 준수)
	checkf(GameModeDataSet, TEXT("UExRunnerMovementComponent: GameModeDataSet이 할당되지 않았습니다. 캐릭터 블루프린트에서 설정해주세요."));

	float MaxYaw = 45.0f;
	if (GameModeDataSet)
	{
		MaxYaw = GameModeDataSet->MaxRunnerYawAngle;
	}
	TargetLookYawOffset = AxisValue * MaxYaw;
}

void UExRunnerMovementComponent::UpdateLanePosition(float DeltaTime)
{
	if (!TargetPawn) return;
	// AutoRun 모드일 때만 동작
	if (!bIsAutoRunMode) return;
	
	// 지연 계산(Lazy Eval): 초기화 시점에 바닥 청크 스폰이 안 되어 실패했다면 매 Tick마다 재시도
	if (!bIsLaneWidthCalculated)
	{
		TArray<AActor*> FloorChunks;
		UGameplayStatics::GetAllActorsOfClass(this, AExFloorChunk::StaticClass(), FloorChunks);
		if (FloorChunks.Num() > 0)
		{
			if (AExFloorChunk* Chunk = Cast<AExFloorChunk>(FloorChunks[0]))
			{
				float TotalWidth = Chunk->GetFloorBounds().GetSize().Y;
				LaneWidth = TotalWidth / 3.0f;
				bIsLaneWidthCalculated = true; // 계산 완료 마킹
				UE_LOG(LogExRunnerMovement, Log, TEXT("[ExRunnerMovement] LaneWidth lazily calculated in Tick: %.1f (TotalWidth: %.1f)"), LaneWidth, TotalWidth);
			}
		}
	}

	// 목표 레인 오프셋 계산 (가운데: 0, 왼쪽: -LaneWidth, 오른쪽: +LaneWidth)
	float TargetY = CurrentLaneIndex * LaneWidth;
	
	// 현재 오프셋 보간 (DeltaTime과 LaneChangeSpeed를 이용해 부드럽게 TargetY를 향해 값 축적)
	CurrentLaneYOffset = FMath::FInterpTo(CurrentLaneYOffset, TargetY, DeltaTime, LaneChangeSpeed);
	
	// [수정] AddActorWorldOffset 위치 강제 갱신 로직 삭제
	// 이유: Mover 시스템이 해당 프레임 마지막에 물리/속도 연산 후 위치를 강제로 덮어씌워버리기 때문에 
	// 틱에서 WorldOffset을 더하는 것은 무시되거나 물리 충돌 버그를 야기함.
	// 대신 위에서 보간된 CurrentLaneYOffset을 ProduceInput_Implementation에서 참조하여 
	// Mover DirectionalInput 방향 벡터(목표 방향성)를 통해 매끄럽게 레인에 스티어링(Steering) 되도록 통합함.
}

void UExRunnerMovementComponent::UpdateCharacterRotation(float DeltaTime)
{
	if (!TargetPawn) return;

	AExRunnerGameState* GS = GetWorld()->GetGameState<AExRunnerGameState>();
	if (!GS || !GS->PathManager || !GS->PathManager->CurveConfig) return;

	AController* Controller = TargetPawn->GetController();
	if (!Controller) return;

	// [수정] F8(Eject) 시 에디터 관전 카메라가 강제로 굳어버리는 버그(ControlRotation 덮어쓰기) 방지
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		// 플레이어 컨트롤러지만 현재 화면 뷰(Target)가 이 폰이 아니라면 조작을 중단합니다.
		if (PC->GetViewTarget() != TargetPawn) return;
	}
	else
	{
		// AI 컨트롤러 등일 경우 권한(Authority) 여부만 체크
		if (!TargetPawn->HasAuthority()) return;
	}

	// [Fix] Mover의 OrientationIntent가 캐릭터 회전을 담당하도록, 컨트롤러 회전 강제 의존을 해제합니다.
	TargetPawn->bUseControllerRotationYaw = false;
	TargetPawn->bUseControllerRotationPitch = false;
	TargetPawn->bUseControllerRotationRoll = false;

	// ACharacter인 경우, Movement 컴포넌트 설정도 확인
	if (ACharacter* Character = Cast<ACharacter>(TargetPawn))
	{
		if (UCharacterMovementComponent* CMC = Character->GetCharacterMovement())
		{
			CMC->bOrientRotationToMovement = false;
		}
	}

	// 플레이어 위치 기반 현재 경로 거리
	float PlayerPathDist = GS->RealPlayerPathDistance; 
	
	// 개선: 트레드밀 속도 대신 실제 캐릭터의 Velocity 사용
	float PlayerSpeed = TargetPawn->GetVelocity().Size();
	if (PlayerSpeed < 10.f)
	{
		PlayerSpeed = 600.f;
	}
	
	const float LookAheadAmount = PlayerSpeed * 0.3f; // 0.3초 앞
	const float LookAheadDist = PlayerPathDist + LookAheadAmount;

	FRotator PathDirection = GS->PathManager->GetDirectionAtDistance(PlayerPathDist);
	FRotator TargetRot = GS->PathManager->GetDirectionAtDistance(LookAheadDist);

	// ★ InputComponent에서 관리하는 조이스틱 좌우 오프셋 각도를 경로 타겟 방향에 누적 합산합니다 ★
	// (이제 누적 방식이 폐기되었으므로 곡선의 방향을 단독으로 따릅니다)

	// Lateral Error(횡방향 오차) 계산
	FVector PathPos = GS->PathManager->GetPositionAtDistance(PlayerPathDist);
	FVector PathRight = FRotationMatrix(PathDirection).GetScaledAxis(EAxis::Y);
	FVector ErrorVec = TargetPawn->GetActorLocation() - PathPos;
	float LateralOffset = FVector::DotProduct(ErrorVec, PathRight);

	float DesiredLateralOffset = CurrentLaneYOffset;
	float LateralError = LateralOffset - DesiredLateralOffset;

	float DynamicInterpSpeed = (GameModeDataSet) ? GameModeDataSet->LookInterpSpeed : 8.0f;

	// [개선] SetActorRotation 강제 오버라이드 삭제 완료.
	// 이유: ProduceInput_Implementation에서 주입하는 Inputs.OrientationIntent가
	// Mover의 내부 TurnGenerator를 통해 물리적으로 부드럽게 캐릭터를 회전시킵니다.
	// TickComponent에서 수동으로 RInterpTo를 돌리면 Mover 회전과 충돌하여 심각한 덜덜거림(Jitter)이 발생합니다.

	// 컨트롤러(주로 카메라가 바라보는) 설정
	FRotator CurrentControlRot = Controller->GetControlRotation();
	FRotator TargetControlRot = CurrentControlRot;
	
	TargetControlRot.Yaw = TargetRot.Yaw + TargetLookYawOffset;

	FRotator NewControlRot = FMath::RInterpTo(CurrentControlRot, TargetControlRot, DeltaTime, DynamicInterpSpeed);
	Controller->SetControlRotation(NewControlRot);

	// [수정] 수동 위치 보정(AddActorWorldOffset) 로직은 ProduceInput의 CorrectionInput으로 이전되어 Mover 시뮬레이션에 통합되었습니다.
	// 이를 통해 조향과 이동이 충돌하지 않고 자연스럽게 하나의 시뮬레이션으로 동작합니다.

	DrawDebugMovementInfo(TargetRot, TargetControlRot, CurrentControlRot);
}

void UExRunnerMovementComponent::DrawDebugMovementInfo(const FRotator& TargetRot, const FRotator& TargetControlRot, const FRotator& CurrentControlRot)
{
	if (!TargetPawn) return;

	// ★ 디버그 드로잉 (시각적 회전 디버깅) - 치트 시스템 연동
	UExDebugStateSubsystem* DS = GetWorld()->GetGameInstance()->GetSubsystem<UExDebugStateSubsystem>();
	if (DS && DS->IsCheatEnabled(TAG_Ex_Debug_Movement))
	{
		FVector DrawStart = TargetPawn->GetActorLocation() + FVector(0, 0, 100.0f); // 머리 위쪽에서 시작

		// 1. 기준이 되는 방향 (Path 정면 방향, 파란색)
		FVector BaseDirection = TargetRot.Vector();
		DrawDebugDirectionalArrow(GetWorld(), DrawStart, DrawStart + BaseDirection * 200.0f, 20.0f, FColor::Blue, false, -1.f, 0, 3.0f);

		// 2. 목적 방향 (TargetControlRot, 즉 Path 정면 + Joystick Offset, 빨간색)
		FVector TargetDirection = TargetControlRot.Vector();
		DrawDebugDirectionalArrow(GetWorld(), DrawStart, DrawStart + TargetDirection * 200.0f, 20.0f, FColor::Red, false, -1.f, 0, 5.0f);

		// 3. 현재 컨트롤러의 실제 보간 중인 방향 (CurrentControlRot, 녹색)
		FVector CurrentDirection = CurrentControlRot.Vector();
		DrawDebugDirectionalArrow(GetWorld(), DrawStart, DrawStart + CurrentDirection * 200.0f, 20.0f, FColor::Green, false, -1.f, 0, 3.0f);

		// 참고용 텍스트 (화면에 간단히 목표 Yaw만 띄움)
		if (GEngine)
		{
			FString DebugMsg = FString::Printf(TEXT("BaseYaw: %.1f | TargetYaw: %.1f | CurrentYaw: %.1f"), TargetRot.Yaw, TargetControlRot.Yaw, CurrentControlRot.Yaw);
			GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::Yellow, DebugMsg);
		}
	}
}

void UExRunnerMovementComponent::SetAutoRunMode(bool bEnabled)
{
	bIsAutoRunMode = bEnabled;

	// AutoRun 해제 시 레인 오프셋 초기화 (중앙으로 복귀)
	if (!bEnabled)
	{
		CurrentLaneIndex = 0;
		CurrentLaneYOffset = 0.0f;
	}

	UE_LOG(LogExRunnerMovement, Log, TEXT("[MovementComp] AutoRunMode = %s"), bEnabled ? TEXT("ON") : TEXT("OFF"));
}

void UExRunnerMovementComponent::OnLaneChangeRequestedCallback(int32 LaneDirection)
{
	// 디버그
	if (GEngine) GEngine->AddOnScreenDebugMessage(83, 2.0f, FColor::Orange, FString::Printf(TEXT("[MovementComp] Received Lane Change: %d"), LaneDirection));

	// LaneDirection: -1 = 좌측, +1 = 우측
	if (LaneDirection < 0)
	{
		MoveLeft();
	}
	else if (LaneDirection > 0)
	{
		MoveRight();
	}
}

void UExRunnerMovementComponent::ApplyPreJumpRotation()
{
	if (!TargetPawn || !bIsAutoRunMode) return;

	AExRunnerGameState* GS = GetWorld()->GetGameState<AExRunnerGameState>();
	if (!GS || !GS->PathManager || !GameModeDataSet) return;

	float PlayerDist = GS->RealPlayerPathDistance;
	
	// 점프 예상 체공 거리 (예: 점프 속도 * 점프 시간) -> 대략 600 유닛 앞
	float LookAheadDist = PlayerDist + 600.0f; 

	FRotator CurrentPathRot = GS->PathManager->GetDirectionAtDistance(PlayerDist);
	FRotator FuturePathRot = GS->PathManager->GetDirectionAtDistance(LookAheadDist);

	// 현재 곡선과 미래 곡선 사이의 휨 정도(Delta Yaw)
	float DeltaYaw = FRotator::NormalizeAxis(FuturePathRot.Yaw - CurrentPathRot.Yaw);

	// 추가 회전 가중치 적용
	float Weight = GameModeDataSet->JumpYawPredictionWeight;
	float FinalYaw = CurrentPathRot.Yaw + (DeltaYaw * Weight);

	// 액터 본체 회전 적용 (점프 개시 시 이 정면 각을 기준으로 날아감)
	FRotator NewRot = TargetPawn->GetActorRotation();
	NewRot.Yaw = FinalYaw;
	TargetPawn->SetActorRotation(NewRot);

	// 탑승된 컨트롤러(카메라) 화면 회전도 동기화 지연 방지
	if (AController* PC = TargetPawn->GetController())
	{
		FRotator ControlRot = PC->GetControlRotation();
		ControlRot.Yaw = FinalYaw;
		PC->SetControlRotation(ControlRot);
	}

	// UE_LOG(LogTemp, Log, TEXT("[PreJump] DeltaYaw: %.1f -> Multiplied FinalYaw Offset: %.1f"), DeltaYaw, DeltaYaw * Weight);
}

