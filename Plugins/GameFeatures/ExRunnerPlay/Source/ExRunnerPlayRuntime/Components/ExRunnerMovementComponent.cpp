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

			if (UExRunnerInputComponent* InputComp = ParentPawn->FindComponentByClass<UExRunnerInputComponent>())
			{
				BindLookInput(InputComp);
				bIsLookInputBound = true;
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
	// [피드백 반영 최종 수정] 
	// Mover의 물리적 방향과 모델 지향 방향을 결정하는 핵심 벡터 도출.
	// 1. 경로 정면(ForwardDir)을 회전축(UpVector) 기준으로 TargetLookYawOffset(조이스틱 비율*최대각도) 만큼 회전시킵니다.
	FVector GoalDirection = ForwardDir;
	if (!FMath::IsNearlyZero(TargetLookYawOffset, 0.1f))
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
	// [수정] GameModeDataSet이 에디터(BP)에서 할당되지 않았을 경우를 대비한 안전 장치.
	// 할당되어 있다면 그 값(0 포함)을 온전히 따르고, 아예 Null이라면 기본값 45도를 사용합니다.
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
		// TargetPawn->AddActorWorldOffset(DeltaMove, true, &SweepHit);
		
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

	// [최종 수정] Mover 플러그인이 ProduceInput의 OrientationIntent를 통해 
	// 알아서 부드럽게 모델을 회전(보간)시키므로, Tick에서 강제로 컨트롤러를 RInterpTo로 비틀어버리는 로직을 완전히 삭제합니다.
	// 이로써 조작 컨트롤러와 물리 Mover 간의 회전 경합(덜덜거림)이 완벽히 해결됩니다.

	// 컨트롤러의 회전은 기본적으로 경로 정면(PathYaw)을 기준으로 유지하되, 
	// 실제 눈에 보이는 캐릭터의 부드러운 회전은 Mover에게 온전히 위임합니다.
	FRotator CurrentControlRot = Controller->GetControlRotation();
	FRotator TargetControlRot = CurrentControlRot;
	
	TargetControlRot.Yaw = TargetRot.Yaw + TargetLookYawOffset;

	// (주의) Mover가 알아서 보간하므로 Tick 단위의 조잡한 RInterpTo 등은 호출하지 않습니다.
	// 단, 카메라 등 컨트롤러 회전에 의존하는 요소가 튀는 것을 방지하기 위해 컨트롤러의 타겟 방향만 
	// 스냅(Snap) 또는 Mover가 회전한 만큼만 따라가게 두셔도 무방합니다. 
	// 여기서는 카메라 등이 타겟을 부드럽게 비출 수 있게만 최소한의 보간을 남겨둡니다 (캐릭터 모델 덜덜거림과는 무관함).
	
	float DynamicInterpSpeed = (GameModeDataSet) ? GameModeDataSet->LookInterpSpeed : 8.0f;
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
