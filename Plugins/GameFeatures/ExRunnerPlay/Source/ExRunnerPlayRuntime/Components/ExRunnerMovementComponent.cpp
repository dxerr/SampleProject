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
#include "Util/Actor/ExActorUtil.h"
#include "ExRunnerInputComponent.h"
#include "Data/Modes/ExGameModeDataSet.h"
#include "SentrySubsystem.h"
#include "SentryLibrary.h"

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
	if (USentrySubsystem* SentrySubsystem = GEngine->GetEngineSubsystem<USentrySubsystem>())
	{
		SentrySubsystem->AddBreadcrumbWithParams(TEXT("BeginPlay Started"), TEXT("ExRunnerMovement"), TEXT("lifecycle"), {});
	}
	
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

			// [진단] 현재 Mover에 등록된 모든 입력 프로듀서를 확인하여 충돌 대상을 식별합니다.
			for (UObject* Producer : MoverComp->InputProducers)
			{
				if (Producer)
				{
					UE_LOG(LogExRunnerMovement, Warning, TEXT("ExRunnerMovement: Registered Input Producer: %s"), *Producer->GetName());
				}
			}

			UE_LOG(LogExRunnerMovement, Warning, TEXT("ExRunnerMovement: 상위 Pawn '%s'의 MoverComponent에 등록 완료 및 Walking 모드(%s) 강제 요청"), *ParentPawn->GetName(), *DefaultModeNames::Walking.ToString());

			if (USentrySubsystem* SentrySubsystem = GEngine->GetEngineSubsystem<USentrySubsystem>())
			{
				// 초기화 완료 시점에 지금까지의 Breadcrumbs를 포함한 진단 리포트 강제 발송
				SentrySubsystem->CaptureMessage(TEXT("Movement Component Diagnostic Report"), ESentryLevel::Info);
			}

			// ExRunnerInputComponent를 동일한 Pawn에서 탐색하여 Look 델리게이트를 자동 바인딩합니다.
			// 블루프린트에서 BindLookInput을 수동 호출할 필요가 없습니다.
			if (UExRunnerInputComponent* InputComp = ParentPawn->FindComponentByClass<UExRunnerInputComponent>())
			{
				BindLookInput(InputComp);
				UE_LOG(LogExRunnerMovement, Log, TEXT("ExRunnerMovement: ExRunnerInputComponent 탐색 성공, OnLookRequested 자동 바인딩 완료"));
			}
			else
			{
				UE_LOG(LogExRunnerMovement, Warning, TEXT("ExRunnerMovement: ExRunnerInputComponent를 찾지 못했습니다. Look 바인딩 생략."));
			}
			
			// 성공 시 타이머 안전 종료
			GetWorld()->GetTimerManager().ClearTimer(InitTimerHandle);
		}
	}
}

void UExRunnerMovementComponent::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	FCharacterDefaultInputs& Inputs = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();
	
	// [개선] 컨트롤러 회전 지연 문제를 해결하기 위해, 경로 매니저로부터 직접 현재 정면(Forward) 벡터를 가져옵니다.
	// 이는 DrawDebugCoordinateSystem의 Red 축 방향과 일치하며, 물리적으로 가장 정확한 'W' 키 입력을 재현합니다.
	FVector ForwardDir = FVector::ForwardVector;
	AExRunnerGameState* GS = GetWorld()->GetGameState<AExRunnerGameState>();
	if (GS && GS->PathManager)
	{
		float PlayerPathDist = GS->RealPlayerPathDistance;
		FRotator PathRot = GS->PathManager->GetDirectionAtDistance(PlayerPathDist);
		ForwardDir = PathRot.Vector(); // 이것이 바로 캐릭터가 나아가야 할 'Red 축' 방향입니다.
	}

	// [입력 주입] IA_Move를 Vector2D(정규화된 방향)로 주입하여 데이터 유실을 방지합니다.
	if (UExRunnerInputComponent* InputComp = TargetPawn ? TargetPawn->FindComponentByClass<UExRunnerInputComponent>() : nullptr)
	{
		// [수정] float -> FVector2D 변환에 맞춰 전진 방향(Y=1.0)을 주입합니다.
		InputComp->RequestMoveAction(FVector2D(0.0f, 1.0f)); 
	}

	// [진단] 현재 Mover 상태 및 이동 모드 설정 주기적 확인
	static double LastProducerLogTime = 0.0;
	double CurrentTimeForProducer = FPlatformTime::Seconds();
	if (CurrentTimeForProducer - LastProducerLogTime > 5.0)
	{
		if (TargetPawn)
		{
			if (UMoverComponent* MoverComp = TargetPawn->FindComponentByClass<UMoverComponent>())
			{
				for (UObject* Producer : MoverComp->InputProducers)
				{
					if (Producer)
					{
						UE_LOG(LogExRunnerMovement, Log, TEXT("[MoverStat] Active Producer: %s"), *Producer->GetName());
					}
				}
				
				UPrimitiveComponent* MovementBase = MoverComp->GetMovementBase();
				FHitResult FloorHit;
				bool bHasFloor = MoverComp->TryGetFloorCheckHitResult(FloorHit);

				// 무브먼트 모드 데이터 추출 (속도 제한 설정 확인용)
				// 일반적으로 Walking 모드 등에서 MaxSpeed 등을 가져올 수 있음
				float CurrentMaxSpeed = 0.0f;
				if (const UCharacterMovementComponent* CMC = TargetPawn->FindComponentByClass<UCharacterMovementComponent>())
				{
					CurrentMaxSpeed = CMC->MaxWalkSpeed;
				}

				UE_LOG(LogExRunnerMovement, Warning, TEXT("[MoverStat] Mode: %s | MaxSpeed: %.1f | Loc: %s | Vel: %s | Base: %s | Floor: %s"), 
					*MoverComp->GetMovementModeName().ToString(),
					CurrentMaxSpeed,
					*TargetPawn->GetActorLocation().ToString(),
					*TargetPawn->GetVelocity().ToString(),
					MovementBase ? *MovementBase->GetName() : TEXT("None"),
					bHasFloor ? (FloorHit.GetActor() ? *FloorHit.GetActor()->GetName() : TEXT("NoActor")) : TEXT("NoHit"));
			}
		}
		LastProducerLogTime = CurrentTimeForProducer;
	}
	// [입력 병합] 수동 입력(Inputs.GetMoveInput()) + 경로 기반 물리 정면 벡터(ForwardDir)
	// Inputs.GetMoveInput()에는 위에서 주입한 IA_Move(0, 1) 성분 등이 Mover 내부 시스템을 통해 녹아들어 있습니다.
	FVector MergedInput = Inputs.GetMoveInput() + ForwardDir;

	if (MergedInput.SizeSquared() > 1.0f)
	{
		MergedInput.Normalize();
	}

	UMoverDataModelBlueprintLibrary::SetDirectionalInput(Inputs, MergedInput);

	// 시각적 지향 방향(OrientationIntent)도 경로 정면(Red 축)으로 일치시킵니다.
	if (!ForwardDir.IsNearlyZero())
	{
		Inputs.OrientationIntent = ForwardDir.GetSafeNormal();
	}

	// 모바일 환경 원인 파악용 로그 (스레드 안전)
	static double LastLogTime = 0.0;
	double CurrentTime = FPlatformTime::Seconds();
	
	if (CurrentTime - LastLogTime > 1.0) // 1초에 한 번만 출력하여 스팸 방지
	{
		UE_LOG(LogExRunnerMovement, Warning, TEXT("[MoveInput] ProduceInput Called! ForwardDir: %s"), *ForwardDir.ToString());
		
		if (USentrySubsystem* SentrySubsystem = GEngine->GetEngineSubsystem<USentrySubsystem>())
		{
			TMap<FString, FSentryVariant> Context;
			Context.Add(TEXT("ForwardX"), static_cast<float>(ForwardDir.X));
			Context.Add(TEXT("ForwardY"), static_cast<float>(ForwardDir.Y));
			SentrySubsystem->AddBreadcrumbWithParams(TEXT("ProduceInput Tick"), TEXT("ExRunnerMovement"), TEXT("input"), Context);
		}
		
		LastLogTime = CurrentTime;
	}
}

void UExRunnerMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 아직 부모 폰에 안 붙었다면 위치 업데이트 로직 등은 스킵
	if (!TargetPawn) return;

	// 속도 수집은 ExRunnerStatComponent의 자체 타이머(StatPollInterval)가 담당합니다.

	// 1. 캐릭터 조향 업데이트 (경로 추적)
	UpdateCharacterRotation(DeltaTime);

	// 2. 레인 변경 처리 (보간)
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
	if (GameModeDataSet)
	{
		// 입력값(AxisValue)을 누적하여 TargetLookYawOffset을 갱신합니다.
		TargetLookYawOffset += AxisValue * GameModeDataSet->RunnerLookSensitivity;
	}
	UE_LOG(LogExRunnerMovement, Warning, TEXT("[LookInput] AxisValue: %.3f → TargetLookYawOffset: %.1f°"), AxisValue, TargetLookYawOffset);
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

void UExRunnerMovementComponent::UpdateCharacterRotation(float DeltaTime)
{
	if (!TargetPawn) return;

	AExRunnerGameState* GS = GetWorld()->GetGameState<AExRunnerGameState>();
	if (!GS || !GS->PathManager || !GS->PathManager->CurveConfig) return;

	AController* Controller = TargetPawn->GetController();
	if (!Controller) return;

	// IsLocallyControlled 거나 HasAuthority일 때만 컨트롤러 회전을 조작합니다.
	if (!TargetPawn->IsLocallyControlled() && !TargetPawn->HasAuthority()) return;

	// [Fix] 캐릭터가 Controller 회전을 따르도록 설정 강제
	TargetPawn->bUseControllerRotationYaw = true;
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

	// ★ [수정] 강제 회전 방지 및 수동 조작 우선 처리 ★
	// 컨트롤러의 Yaw 회전은 플레이어의 수동 조각(BP AddControllerYawInput)이 전적으로 담당해야 합니다.
	FRotator CurrentControlRot = Controller->GetControlRotation();
	FRotator NewControlRot = CurrentControlRot;

	// ★ [핵심 수정] 절대 Yaw 오프셋 기반 보간 방식 ★
	// 각도 보간 시 RInterpTo를 사용하여 현재 컨트롤러 회전에서 목표 회전으로 최단 거리(Shortest Path) 부드럽게 보간합니다.
	
	// 1. 경로 정면(TargetRot.Yaw)을 기준으로 플랫폼 스와이프 오프셋(TargetLookYawOffset)을 더한 목표 회전값 세팅
	FRotator TargetControlRot = CurrentControlRot;
	TargetControlRot.Yaw = TargetRot.Yaw + TargetLookYawOffset;

	// 2. 보간 속도를 환경에 맞춰 조정 (곡선에서는 더 빠르게 반응하도록 보정 가능)
	float DynamicInterpSpeed = (GameModeDataSet) ? GameModeDataSet->LookInterpSpeed : 8.0f;
	
	// 현재 Yaw와 목표 Yaw의 차이가 크면(급커브) 보간 속도를 일시적으로 높여 반응성을 확보합니다.
	float YawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentControlRot.Yaw, TargetControlRot.Yaw));
	if (YawDelta > 15.0f)
	{
		DynamicInterpSpeed *= 1.5f;
	}

	NewControlRot = FMath::RInterpTo(CurrentControlRot, TargetControlRot, DeltaTime, DynamicInterpSpeed);

	// UE_LOG(LogExRunnerMovement, Warning, TEXT("[LookInput] PathYaw: %.1f | TargetLookYawOffset: %.1f | FinalTargetYaw: %.1f | CurrentYaw: %.1f → NewYaw: %.1f"),
	//	TargetRot.Yaw, TargetLookYawOffset, TargetControlRot.Yaw, CurrentControlRot.Yaw, NewControlRot.Yaw);

	// Pitch와 Roll은 기존 컨트롤러 회전값으로 덮어써서 Yaw 연산의 영향만 남김
	NewControlRot.Pitch = CurrentControlRot.Pitch;
	NewControlRot.Roll  = CurrentControlRot.Roll;

	Controller->SetControlRotation(NewControlRot);

	// [수정] 수동 위치 보정(AddActorWorldOffset) 로직은 ProduceInput의 CorrectionInput으로 이전되어 Mover 시뮬레이션에 통합되었습니다.
	// 이를 통해 조향과 이동이 충돌하지 않고 자연스럽게 하나의 시뮬레이션으로 동작합니다.

	// ★ 디버그 드로잉 (실제 위치와 경로 위치 간의 차이 가시화)
	DrawDebugCoordinateSystem(GetWorld(), TargetPawn->GetActorLocation(), TargetPawn->GetActorRotation(), 100.f, false, -1.f, 0, 2.f);
	DrawDebugCoordinateSystem(GetWorld(), PathPos, PathDirection, 100.f, false, -1.f, 0, 2.f);
	DrawDebugLine(GetWorld(), TargetPawn->GetActorLocation(), PathPos, FColor::Yellow, false, -1.f, 0, 2.f);

	// 화면 출력 디버깅
	if (GEngine)
	{
		FString DebugMsg = FString::Printf(TEXT("Dist: %.0f | Err: %.0f | PathYaw: %.1f | PlayerYaw: %.1f"), 
			PlayerPathDist, FVector::Dist(TargetPawn->GetActorLocation(), PathPos), PathDirection.Yaw, TargetPawn->GetActorRotation().Yaw);
		GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::Cyan, DebugMsg);

		FString SteeringMsg = FString::Printf(TEXT("Offset: %.1f | FinalYaw: %.1f"), 
			LateralOffset, TargetRot.Yaw);
		GEngine->AddOnScreenDebugMessage(2, 0.f, FColor::Orange, SteeringMsg);
	}
}
