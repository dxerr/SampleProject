// Copyright ExFrameWork. All Rights Reserved.
// 오프셋 기반 트레드밀 시스템: BaseSpeed + 캐릭터 위치 오프셋 보정

#include "ExRunnerGameMode.h"
#include "../Components/ExRunnerMovementComponent.h"
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
	
	// 개선: 트레드밀 속도 대신 실제 캐릭터의 Velocity 사용
	float PlayerSpeed = PlayerPawn->GetVelocity().Size();
	if (PlayerSpeed < 10.f)
	{
		// 움직이지 않을 때는 기본 600 정도를 기준으로 설정
		PlayerSpeed = 600.f;
	}
	
	const float LookAheadAmount = PlayerSpeed * 0.3f; // 0.3초 앞
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

	// 캐릭터가 이동하려는 레인(Lane)의 추가 목표 오프셋
	float DesiredLateralOffset = 0.f;
	if (UExRunnerMovementComponent* MoveComp = PlayerPawn->FindComponentByClass<UExRunnerMovementComponent>())
	{
		DesiredLateralOffset = MoveComp->GetCurrentLaneYOffset();
	}
	float LateralError = LateralOffset - DesiredLateralOffset;

	// 3. P-Control Steering
	// 오차에 비례하여 반대 방향으로 회전 보정
	// Gain: 클수록 강하게 보정하지만, 너무 크면 진동 발생 (-0.1 ~ -0.5 추천)
	const float SteeringGain = -0.15f; 
	float SteeringYaw = LateralError * SteeringGain;
	
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

	if (bRunnerModeEnabled)
	{
		StartRunnerGame();
	}
}

void AExRunnerGameMode::StartRunnerGame()
{
	CurrentPathDistance = 0.f;

	// PathManager 초기화
	if (PathManager)
	{
		if (CurveConfig)
		{
			PathManager->CurveConfig = CurveConfig;
		}
		PathManager->InitializePath(FVector::ZeroVector, FRotator::ZeroRotator);
		// 초기 경로 오프셋이 필요하다면 여기서 설정할 수도 있음.
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

	UE_LOG(LogExRunnerPlay, Log, TEXT("ExRunnerGameMode Started (World Moved Mode)"));
}

void AExRunnerGameMode::StopRunnerGame()
{
	bRunnerModeEnabled = false;
	UE_LOG(LogExRunnerPlay, Log, TEXT("ExRunnerGameMode: Runner Game Stopped."));
}


void AExRunnerGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bRunnerModeEnabled)
	{
		return;
	}

	// 핵심 갱신: 플레이어 폰 가져오기
	APawn* PlayerPawn = GetCachedPlayerPawn();
	if (!PlayerPawn)
	{
		return;
	}

	// 실제 이동한 거리를 RealPlayerPathDistance에 갱신
	// 플레이어 위치를 기준으로 가장 가까운 경로 상의 거리(투영)를 계산하여 추적합니다.
	FVector PlayerLocation = PlayerPawn->GetActorLocation();
	RealPlayerPathDistance = PathManager->GetClosestDistanceAtLocation(PlayerLocation, RealPlayerPathDistance, 2000.f);
	
	// 가상 경로 거리도 동기화 (트레드밀/LookAhead 참조 등에 사용)
	CurrentPathDistance = RealPlayerPathDistance;

	// 1. 캐릭터 회전 갱신 (경로 접선 방향으로 커브 제어)
	UpdateCharacterRotation(DeltaTime);
	
	// 2. 디버그 및 진행 모니터링 기능은 별도 갱신 필요시 추가
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




