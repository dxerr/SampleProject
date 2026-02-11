// Copyright ExFrameWork. All Rights Reserved.
// 오프셋 기반 트레드밀 시스템: BaseSpeed + 캐릭터 위치 오프셋 보정

#include "ExRunnerGameMode.h"
#include "../Components/ExChunkSpawner.h"
#include "../Components/ExObstacleManager.h"
#include "../Actors/ExFloorChunk.h"
#include "ExGameplayTags.h"
#include "ExGameplayEventSubsystem.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogExRunnerPlay, Log, All);

AExRunnerGameMode::AExRunnerGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	// NOTE: 기본 TickGroup(TG_PrePhysics) 사용
	// 오프셋 기반 보정이므로 X 리셋이 없어 Tick 순서에 민감하지 않음

	ChunkSpawner = CreateDefaultSubobject<UExChunkSpawner>(TEXT("ChunkSpawner"));
	ObstacleManager = CreateDefaultSubobject<UExObstacleManager>(TEXT("ObstacleManager"));
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

	// 1. 기준점(TargetX) 설정 (첫 프레임 또는 재개 시)
	if (!bTrackingInitialized)
	{
		TargetX = CurrentX;
		bTrackingInitialized = true;
		UE_LOG(LogExRunnerPlay, Log, TEXT("Treadmill Tracking Initialized: TargetX=%.1f"), TargetX);
	}

	// 2. 오프셋 계산 (캐릭터가 기준점보다 앞 = 양수)
	// 양수: 트레드밀 빨라져야 함 / 음수: 트레드밀 느려져야 함
	float Offset = CurrentX - TargetX;

	// 3. 목표 속도 계산 (P-Control)
	// BaseSpeed를 기준으로 오프셋만큼 가감속
	float TargetSpeed = BaseTreadmillSpeed + (Offset * CorrectionStrength);

	// 최소 속도 0 보장
	TargetSpeed = FMath::Max(TargetSpeed, 0.f);

	// 4. 부드러운 속도 갱신 (Interpolation)
	// 급격한 속도 변화를 방지하여 부드러운 움직임 구현
	CurrentTreadmillSpeed = FMath::FInterpTo(CurrentTreadmillSpeed, TargetSpeed, DeltaTime, 2.0f);

	// 5. Floor 이동
	if (ChunkSpawner)
	{
		ChunkSpawner->ShiftWorld(-CurrentTreadmillSpeed * DeltaTime);
	}

	// 6. 거리 누적
	TotalDistance += CurrentTreadmillSpeed * DeltaTime;
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
