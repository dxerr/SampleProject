// Copyright ExFrameWork. All Rights Reserved.
// 오프셋 기반 트레드밀 시스템: BaseSpeed + 캐릭터 위치 오프셋 보정

#include "ExRunnerGameMode.h"
#include "../Components/ExChunkSpawner.h"
#include "../Components/ExObstacleManager.h"
#include "../Components/ExPathManager.h"
#include "../Data/ExCurveConfig.h"
#include "../Actors/ExFloorChunk.h"
#include "ExGameplayTags.h"
#include "ExGameplayEventSubsystem.h"
#include "ExDebugStateSubsystem.h"
#include "Kismet/GameplayStatics.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h" // 디버그 드로잉용

DEFINE_LOG_CATEGORY_STATIC(LogExRunnerPlay, Log, All);

// ... (Existing Include)

void AExRunnerGameMode::UpdateCharacterRotation(float DeltaTime)
{
	if (!PathManager || !CurveConfig) return;

	APawn* PlayerPawn = GetCachedPlayerPawn();
	if (!PlayerPawn) return;

	AController* Controller = PlayerPawn->GetController();
	if (!Controller) return;

	// ★ 각도 계산 보정: 캐릭터가 (0,0,0)이 아닌 오프셋 위치(TargetX 전후)에 있을 수 있음.
	// 기존: CurrentPathDistance + X (직선 가정, 곡선에서 부정확)
	// 개선: 실제 월드 위치를 경로에 투영하여 정확한 경로 거리 산출
	float PlayerPathDist = PathManager->GetClosestDistanceAtLocation(PlayerPawn->GetActorLocation(), CurrentPathDistance, 3000.f);

	// ★ 실제 플레이어 거리 저장 (Chunk 삭제 판단용)
	RealPlayerPathDistance = PlayerPathDist;

	// 현재 경로 거리에서의 접선 방향 및 위치 조회
	FRotator PathDirection = PathManager->GetDirectionAtDistance(PlayerPathDist);
	FVector ExpectedPos = PathManager->GetPositionAtDistance(PlayerPathDist);
	FVector ActualPos = PlayerPawn->GetActorLocation();

	// ★ 디버그 드로잉 (빨강=실제, 초록=계산된 경로 위치)
	if (bRunnerModeEnabled) // 너무 많으면 정신없으니 모드 체크
	{
		DrawDebugCoordinateSystem(GetWorld(), ActualPos, PlayerPawn->GetActorRotation(), 100.f, false, -1.f, 0, 2.f);
		DrawDebugCoordinateSystem(GetWorld(), ExpectedPos, PathDirection, 100.f, false, -1.f, 0, 2.f);
		DrawDebugLine(GetWorld(), ActualPos, ExpectedPos, FColor::Yellow, false, -1.f, 0, 2.f);

		// 화면 출력 디버깅
		if (GEngine)
		{
			FString DebugMsg = FString::Printf(TEXT("Dist: %.0f | Err: %.0f | PathYaw: %.1f | PlayerYaw: %.1f"), 
				PlayerPathDist, FVector::Dist(ActualPos, ExpectedPos), PathDirection.Yaw, PlayerPawn->GetActorRotation().Yaw);
			GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::Cyan, DebugMsg);
		}
	}

	// ★ Controller Yaw 회전 처리 (카메라 연동)
	// 캐릭터 자체 회전 대신 컨트롤러 회전을 사용해야 SpringArm 등 카메라가 따라옴.
	FRotator CurrentControlRot = Controller->GetControlRotation();

	// [Fix] 캐릭터가 Controller 회전을 따르도록 설정 강제
	PlayerPawn->bUseControllerRotationYaw = true;
	PlayerPawn->bUseControllerRotationPitch = false;
	PlayerPawn->bUseControllerRotationRoll = false;

	// ACharacter인 경우, Movement 컴포넌트 설정도 확인
	if (ACharacter* Character = Cast<ACharacter>(PlayerPawn))
	{
		if (UCharacterMovementComponent* CMC = Character->GetCharacterMovement())
		{
			CMC->bOrientRotationToMovement = false;
		}
	}

	// ──────────────────────────────────────────────
	// [Steering Correction] 조향 보정 로직
	// ──────────────────────────────────────────────
	// 1. Lookahead: 반응 지연 보상을 위해 조금 앞의 경로를 조회
	const float LookAheadAmount = CurrentTreadmillSpeed * 0.3f; // 0.3초 앞
	const float LookAheadDist = PlayerPathDist + LookAheadAmount;

	FRotator TargetRot = PathManager->GetDirectionAtDistance(LookAheadDist);

	// 2. Lateral Error(횡방향 오차) 계산
	// 현재 위치에서 가장 가까운 경로상의 점
	FVector PathPos = PathManager->GetPositionAtDistance(PlayerPathDist);
	// 경로의 오른쪽 벡터 (Yaw + 90)
	FVector PathRight = FRotationMatrix(PathDirection).GetScaledAxis(EAxis::Y);
	// 플레이어가 경로 중심에서 얼마나 오른쪽/왼쪽에 있는지 (Right +, Left -)
	FVector ErrorVec = PlayerPawn->GetActorLocation() - PathPos;
	float LateralOffset = FVector::DotProduct(ErrorVec, PathRight);

	// 3. P-Control Steering
	// 오차에 비례하여 반대 방향으로 회전 보정
	// Gain: 클수록 강하게 보정하지만, 너무 크면 진동 발생 (-0.1 ~ -0.5 추천)
	const float SteeringGain = -0.15f; 
	float SteeringYaw = LateralOffset * SteeringGain;
	
	// 과도한 회전 방지 (Clamp)
	SteeringYaw = FMath::Clamp(SteeringYaw, -15.f, 15.f);

	// 최종 목표 회전에 보정값 적용
	TargetRot.Yaw += SteeringYaw;

	// 4. 부드러운 보간 (RInterpTo)
	// Steering을 적용했으므로 보간 속도를 조금 더 빠르게 해도 됨
	FRotator NewControlRot = FMath::RInterpTo(
		CurrentControlRot,
		TargetRot,
		DeltaTime,
		CurveConfig->CharacterRotationInterpSpeed * 1.5f // 반응성 향상
	);

	// Yaw만 적용 (Pitch/Roll은 카메라 제어권 유지)
	NewControlRot.Pitch = CurrentControlRot.Pitch;
	NewControlRot.Roll = CurrentControlRot.Roll;

	Controller->SetControlRotation(NewControlRot);

	// 캐릭터 자체는 상하 기울어지지 않고 이동 방향(Yaw)만 바라보도록 강제 회전
	FRotator PawnFlatRot = PlayerPawn->GetActorRotation();
	PawnFlatRot.Yaw = NewControlRot.Yaw;
	PlayerPawn->SetActorRotation(PawnFlatRot);

	// 디버그 출력 업데이트
	if (bRunnerModeEnabled && GEngine)
	{
		FString SteeringMsg = FString::Printf(TEXT("Offset: %.1f | Steer: %.1f | FinalYaw: %.1f"), 
			LateralOffset, SteeringYaw, TargetRot.Yaw);
		GEngine->AddOnScreenDebugMessage(2, 0.f, FColor::Orange, SteeringMsg);
	}
}

AExRunnerGameMode::AExRunnerGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	// NOTE: 기본 TickGroup(TG_PrePhysics) 사용
	// 오프셋 기반 보정이므로 X 리셋이 없어 Tick 순서에 민감하지 않음

	ChunkSpawner = CreateDefaultSubobject<UExChunkSpawner>(TEXT("ChunkSpawner"));
	ObstacleManager = CreateDefaultSubobject<UExObstacleManager>(TEXT("ObstacleManager"));
	PathManager = CreateDefaultSubobject<UExPathManager>(TEXT("PathManager"));
}

void AExRunnerGameMode::BeginPlay()
{
	Super::BeginPlay();

	// GameplayTag 이벤트 등록
	if (UExGameplayEventSubsystem* EventSub = GetWorld()->GetSubsystem<UExGameplayEventSubsystem>())
	{
		EventSub->GetEventDelegate(TAG_Ex_Action_Climb_Start).AddDynamic(this, &AExRunnerGameMode::OnClimbStart);
		EventSub->GetEventDelegate(TAG_Ex_Action_Climb_End).AddDynamic(this, &AExRunnerGameMode::OnClimbEnd);
		UE_LOG(LogExRunnerPlay, Log, TEXT("ExRunnerGameMode: Registered GameplayTag event listeners"));
	}

	if (bRunnerModeEnabled)
	{
		StartRunnerGame();
	}
}

void AExRunnerGameMode::StartRunnerGame()
{
	CurrentTreadmillSpeed = BaseTreadmillSpeed;
	TotalDistance = 0.f;
	bTreadmillPaused = false;
	bTreadmillDisabled = false;
	bTrackingInitialized = false;
	CurrentPathDistance = 0.f;

	// PathManager 초기화
	if (PathManager)
	{
		if (CurveConfig)
		{
			PathManager->CurveConfig = CurveConfig;
		}
		PathManager->InitializePath(FVector::ZeroVector, FRotator::ZeroRotator);
		UE_LOG(LogExRunnerPlay, Log, TEXT("PathManager 초기화 완료 (CurveConfig=%s)"),
			CurveConfig ? *CurveConfig->GetName() : TEXT("None"));
	}

	if (ChunkSpawner)
	{
		ChunkSpawner->InitializeSpawner();
	}

	if (ObstacleManager && ChunkSpawner)
	{
		ObstacleManager->BindToSpawner(ChunkSpawner);
	}

	UE_LOG(LogExRunnerPlay, Log, TEXT("ExRunnerGameMode Started (Offset Mode) - BaseSpeed=%.0f, Correction=%.1f"),
		BaseTreadmillSpeed, CorrectionStrength);
}

void AExRunnerGameMode::StopRunnerGame()
{
	CurrentTreadmillSpeed = 0.f;
	bRunnerModeEnabled = false;
	bTrackingInitialized = false;
	UE_LOG(LogExRunnerPlay, Log, TEXT("ExRunnerGameMode: Runner Game Stopped."));
}

void AExRunnerGameMode::SetTreadmillPaused(bool bPaused)
{
	if (bTreadmillPaused != bPaused)
	{
		bTreadmillPaused = bPaused;

		if (!bPaused)
		{
			// ★ 재개 시: 현재 위치를 새 TargetX로 갱신
			// Climb 등으로 캐릭터가 이동했을 수 있으므로
			bTrackingInitialized = false;
		}

		UE_LOG(LogExRunnerPlay, Log, TEXT("Treadmill Paused: %s"), bPaused ? TEXT("True") : TEXT("False"));
	}
}

void AExRunnerGameMode::SetTreadmillDisabled(bool bDisabled)
{
	if (bTreadmillDisabled != bDisabled)
	{
		bTreadmillDisabled = bDisabled;

		if (!bDisabled)
		{
			bTrackingInitialized = false;
		}
		if (bDisabled)
		{
			CurrentTreadmillSpeed = 0.f;
		}

		UE_LOG(LogExRunnerPlay, Log, TEXT("Treadmill Disabled: %s"), bDisabled ? TEXT("True") : TEXT("False"));
	}
}



// ──────────────────────────────────────────────
// 핵심: 오프셋 기반 트레드밀 Tick
// ──────────────────────────────────────────────
void AExRunnerGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bRunnerModeEnabled || bTreadmillDisabled || bTreadmillPaused)
	{
		return;
	}



	// 2. 플레이어 폰 가져오기
	APawn* PlayerPawn = GetCachedPlayerPawn();
	if (!PlayerPawn)
	{
		// 폰 없으면 기본 속도로 이동
		if (ChunkSpawner)
		{
			ChunkSpawner->ShiftWorld(-BaseTreadmillSpeed * DeltaTime);
		}
		TotalDistance += BaseTreadmillSpeed * DeltaTime;
		CurrentTreadmillSpeed = BaseTreadmillSpeed;
		return;
	}

	const float CurrentX = PlayerPawn->GetActorLocation().X;

	// 1. 기준점(TargetLocation) 설정 (첫 프레임 또는 재개 시)
	if (!bTrackingInitialized)
	{
		TargetLocation = PlayerPawn->GetActorLocation();
		// Z축은 무시 (높이 변동에 의한 속도 영향 제외)
		// TargetLocation.Z = 0.f; // 필요하다면
		
		bTrackingInitialized = true;
		UE_LOG(LogExRunnerPlay, Log, TEXT("Treadmill Tracking Initialized: TargetLoc=%s"), *TargetLocation.ToString());
	}

	// 2. 오프셋 계산 (벡터 내적 기반)
	// 기존: CurrentX - TargetX (직선 전용)
	// 개선: (PlayerPos - TargetPos) • PathTangnet
	// 경로의 진행 방향(Tangent) 성분만큼 얼마나 앞서가고 있는지 판단
	
	// 현재 플레이어 위치 (트레드밀 상의 절대 위치가 아니라, 화면/월드 상의 상대 위치)
	FVector CurrentPos = PlayerPawn->GetActorLocation();
	FVector OffsetVec = CurrentPos - TargetLocation;

	// 현재 경로의 접선 방향 (CurveConfig가 있다면 PathManager에서 조회, 없다면 X축)
	FVector PathDirVector = FVector::ForwardVector;
	if (PathManager && CurveConfig)
	{
		// 플레이어 위치에서의 접선 방향 조회 (이미 UpdateCharacterRotation 등에서 계산됨)
		// 효율을 위해 여기서 다시 계산하거나 캐싱된 값 사용
		// 가장 정확한 건 PlayerPathDist에서의 접선
		// 그러나 Tick 순서 상 아직 UpdateCharacterRotation 전일 수 있음.
		// CurrentPathDistance 기준 접선은 "월드가 이동할 방향" (반대)
		// 플레이어가 이동해야 할 방향은 PathManager->GetDirectionAtDistance(RealPlayerPathDist)
		// 편의상 CurrentPathDistance(0,0,0 근처)의 접선을 사용해도 무방 (TargetLocation이 0,0,0 근처라면)
		
		// ★ 중요: TargetLocation 근처의 접선을 써야 함.
		// Player가 멀리 갔다면 접선이 달라질 수 있음.
		// 하지만 "Treadmill"은 Player를 TargetLocation(고정점)에 묶어두는 것이 목표.
		// 따라서 "TargetLocation에서의 접선 방향"으로 투영하는 것이 타당함?
		// 아니, Player가 90도 꺾인 곳에 있다면 Player 쪽 접선을 써야 함.
		// Player가 진행해야 할 방향으로 얼마나 더 갔는가?
		
		// 1안: TargetLocation에서의 접선. (직관적)
		// 2안: PlayerLocation에서의 접선. (곡선 정밀도)
		
		// 여기서는 CurrentPathDistance(화면 중앙/TargetLocation 근처)의 접선을 사용.
		// 왜냐하면 월드 시프트는 CurrentPathDistance의 접선 반대 방향으로 일어나기 때문.
		PathDirVector = PathManager->GetDirectionAtDistance(CurrentPathDistance).Vector();
	}

	float Offset = FVector::DotProduct(OffsetVec, PathDirVector);

	// 3. 목표 속도 계산 (P-Control)
	float TargetSpeed = BaseTreadmillSpeed + (Offset * CorrectionStrength);

	// 최소 속도 0 보장
	TargetSpeed = FMath::Max(TargetSpeed, 0.f);

	// 4. 부드러운 속도 갱신 (Interpolation)
	// 급격한 속도 변화를 방지하여 부드러운 움직임 구현
	CurrentTreadmillSpeed = FMath::FInterpTo(CurrentTreadmillSpeed, TargetSpeed, DeltaTime, 2.0f);

	// 5. Floor 이동 (경로 기반 또는 레거시)
	const float DeltaDistance = CurrentTreadmillSpeed * DeltaTime;
	if (ChunkSpawner)
	{
		if (PathManager && CurveConfig)
		{
			// 경로 기반 이동
			// 경로 기반 이동 (Global Shift)
			// 플레이어 위치(CurrentPathDistance)에서의 접선 방향으로 전체 월드를 반대로 이동
			FRotator PathDirection = PathManager->GetDirectionAtDistance(CurrentPathDistance);
			FVector ShiftVector = PathDirection.Vector() * (-DeltaDistance);
			
			// Z축 이동(Pitch)은 램프/경사로 구현되므로, 월드 전체를 내리는 것보다는
			// 수평 이동 + 높이차(Z)는 Player Movement가 처리?
			// 아니, 트레드밀에서 경사를 오르려면 월드가 내려가야 함?
			// PathDirection에 Pitch가 포함되어 있다면 Z축도 이동됨.
			// 4분면 시스템에서 Pitch는 Spiral Ramp를 위해 사용됨.
			// 따라서 ShiftVector에 Z가 포함되면 월드가 내려가면서 캐릭터가 상대적으로 올라가는 효과.
			
			ChunkSpawner->ShiftWorldByVector(ShiftVector);
			
			// ★ PathManager의 원점도 함께 이동해야 다음 세그먼트가 올바른 위치(플레이어 근처)에 생성됨
			PathManager->ShiftPathOrigin(ShiftVector);
		}
		else
		{
			// 레거시 직선 이동
			ChunkSpawner->ShiftWorld(-DeltaDistance);
		}
	}

	// 6. 거리 누적
	TotalDistance += DeltaDistance;
	CurrentPathDistance += DeltaDistance;

	// 7. 캐릭터 회전 갱신 (경로 접선 방향)
	UpdateCharacterRotation(DeltaTime);

	// 8. 디버그 시각화 (TAG_Ex_Debug_Speed)
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UExDebugStateSubsystem* DS = GI->GetSubsystem<UExDebugStateSubsystem>())
		{
			if (DS->IsCheatEnabled(TAG_Ex_Debug_Speed))
			{
				if (GEngine)
				{
					FString SpeedMsg = FString::Printf(TEXT("[Cheat] Treadmill Speed: Cur=%.1f / Base=%.1f"), CurrentTreadmillSpeed, BaseTreadmillSpeed);
					// 키 777을 사용하여 같은 라인 갱신되도록 처리, 지속시간 0.0f
					GEngine->AddOnScreenDebugMessage(777, 0.0f, FColor::Green, SpeedMsg, false, FVector2D(1.5f, 1.5f));
				}
			}
		}
	}
}

// ──────────────────────────────────────────────
// PlayerPawn 캐시 헬퍼
// ──────────────────────────────────────────────
APawn* AExRunnerGameMode::GetCachedPlayerPawn()
{
	if (CachedPlayerPawn.IsValid())
	{
		return CachedPlayerPawn.Get();
	}

	APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (Pawn)
	{
		CachedPlayerPawn = Pawn;
	}
	return Pawn;
}

// ========== GameplayTag Event Callbacks ==========
void AExRunnerGameMode::OnClimbStart(FGameplayTag EventTag, const FExGameplayEventPayload& Payload)
{
	UE_LOG(LogExRunnerPlay, Log, TEXT("ExRunnerGameMode: OnClimbStart received from %s"),
		Payload.Instigator ? *Payload.Instigator->GetName() : TEXT("Unknown"));
	SetTreadmillPaused(true);
}

void AExRunnerGameMode::OnClimbEnd(FGameplayTag EventTag, const FExGameplayEventPayload& Payload)
{
	UE_LOG(LogExRunnerPlay, Log, TEXT("ExRunnerGameMode: OnClimbEnd received from %s"),
		Payload.Instigator ? *Payload.Instigator->GetName() : TEXT("Unknown"));
	SetTreadmillPaused(false);
}


