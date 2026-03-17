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
			
			if (USentrySubsystem* SentrySubsystem = GEngine->GetEngineSubsystem<USentrySubsystem>())
			{
				TMap<FString, FSentryVariant> Context;
				Context.Add(TEXT("PawnName"), ParentPawn->GetName());
				
				// 추가 방어 로직: Mover 시스템 준비 상태 체크
				if (MoverComp->MovementModes.Num() > 0)
				{
					Context.Add(TEXT("MovementModesCount"), FString::FromInt(MoverComp->MovementModes.Num()));
					Context.Add(TEXT("CurrentMode"), MoverComp->GetMovementModeName().ToString());
				}
				else
				{
					Context.Add(TEXT("MoverStatus"), TEXT("NO_MODES_FOUND"));
					UE_LOG(LogExRunnerMovement, Warning, TEXT("ExRunnerMovement: MoverComponent에 등록된 이동 모드가 없습니다. (에셋 로딩 실패 의심)"));
				}

				SentrySubsystem->AddBreadcrumbWithParams(TEXT("Mover Initialized"), TEXT("ExRunnerMovement"), TEXT("init"), Context);
			}

			UE_LOG(LogExRunnerMovement, Log, TEXT("ExRunnerMovement: 상위 Pawn '%s'의 MoverComponent에 InputProducer로 등록 완료"), *ParentPawn->GetName());

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
	
	FVector ForwardDir = FVector::ForwardVector;
	FString DebugCtrlStatus = TEXT("NoController");
		
	if (TargetPawn)
	{
		if (AController* PawnController = TargetPawn->GetController())
		{
			const FRotator Rotation = PawnController->GetControlRotation();
			const FRotator YawRotation(0, Rotation.Yaw, 0);
			ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
			DebugCtrlStatus = FString::Printf(TEXT("CtrlYaw:%.1f"), Rotation.Yaw);
		}
	}

	Inputs.SetMoveInput(EMoveInputType::DirectionalIntent, ForwardDir);
	Inputs.OrientationIntent = ForwardDir;

	// [수정] Mover 컴포넌트의 ProduceInput은 워커 스레드(네트워크 시뮬레이션 등)에서 호출될 수 있습니다.
	// GEngine->AddOnScreenDebugMessage는 게임 스레드에서만 안전하므로, 이를 여기서 직접 호출하면 모바일/패키징 빌드에서 치명적인 크래시가 발생할 수 있습니다.
	
	// 모바일 환경 원인 파악용 로그 (스레드 안전)
	static double LastLogTime = 0.0;
	double CurrentTime = FPlatformTime::Seconds();
	
	if (CurrentTime - LastLogTime > 1.0) // 1초에 한 번만 출력하여 스팸 방지
	{
		UE_LOG(LogExRunnerMovement, Warning, TEXT("[MoveInput] ProduceInput Called! ForwardDir: (%.2f, %.2f) | %s"), ForwardDir.X, ForwardDir.Y, *DebugCtrlStatus);
		
		if (USentrySubsystem* SentrySubsystem = GEngine->GetEngineSubsystem<USentrySubsystem>())
		{
			TMap<FString, FSentryVariant> Context;
			Context.Add(TEXT("ForwardX"), static_cast<float>(ForwardDir.X));
			Context.Add(TEXT("ForwardY"), static_cast<float>(ForwardDir.Y));
			Context.Add(TEXT("CtrlStatus"), DebugCtrlStatus);
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

void UExRunnerMovementComponent::OnLookRequestedCallback(float NormX)
{
	const float MaxYaw = (GameModeDataSet) ? GameModeDataSet->MaxRunnerYawAngle : 45.0f;
	TargetLookYawOffset = NormX * MaxYaw;

	UE_LOG(LogExRunnerMovement, Warning, TEXT("[LookInput] NormX: %.3f → TargetLookYawOffset: %.1f°"), NormX, TargetLookYawOffset);
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
	// 각도 보간 시 FInterpTo는 -180 ~ 180도 경계에서 오작동하여 캐릭터가 빙글 도는 현상이 발생합니다.
	// 따라서 항상 최단 방향을 찾아주는 RInterpTo(Rotator 구조체 단위)를 사용해야 합니다.
	
	// 1. 경로 정면(TargetRot.Yaw)을 기준으로 플랫폼 스와이프 오프셋(TargetLookYawOffset)을 더한 목표 회전값 세팅
	FRotator TargetControlRot = CurrentControlRot;
	TargetControlRot.Yaw = TargetRot.Yaw + TargetLookYawOffset;

	// 2. RInterpTo를 사용하여 현재 컨트롤러 회전에서 목표 회전으로 최단 거리(Shortest Path) 부드럽게 보간
	const float InterpSpeed = (GameModeDataSet) ? GameModeDataSet->LookInterpSpeed : 8.0f;
	NewControlRot = FMath::RInterpTo(CurrentControlRot, TargetControlRot, DeltaTime, InterpSpeed);

	// UE_LOG(LogExRunnerMovement, Warning, TEXT("[LookInput] PathYaw: %.1f | TargetLookYawOffset: %.1f | FinalTargetYaw: %.1f | CurrentYaw: %.1f → NewYaw: %.1f"),
	//	TargetRot.Yaw, TargetLookYawOffset, TargetControlRot.Yaw, CurrentControlRot.Yaw, NewControlRot.Yaw);

	// Pitch와 Roll은 기존 컨트롤러 회전값으로 덮어써서 Yaw 연산의 영향만 남김
	NewControlRot.Pitch = CurrentControlRot.Pitch;
	NewControlRot.Roll  = CurrentControlRot.Roll;

	Controller->SetControlRotation(NewControlRot);

	// 물리적 횡이동(Drift) 원심력 보정
	if (FMath::Abs(LateralError) > 1.0f)
	{
		const float DriftCorrectionSpeed = 15.0f; // 밀려나는 것을 잡아주는 인력 강도
		FVector CorrectionDelta = -PathRight * (LateralError * DriftCorrectionSpeed * DeltaTime);
		TargetPawn->AddActorWorldOffset(CorrectionDelta, true);
	}

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
