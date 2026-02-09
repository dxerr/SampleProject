// Copyright ExFrameWork. All Rights Reserved.

#include "ExRunnerGameMode.h"
#include "../Components/ExChunkSpawner.h"
#include "../Components/ExObstacleManager.h"
#include "ExGameplayTags.h"
#include "ExGameplayEventSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogExRunnerPlay, Log, All);

AExRunnerGameMode::AExRunnerGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

	// 청크 스포너 컴포넌트 생성
	ChunkSpawner = CreateDefaultSubobject<UExChunkSpawner>(TEXT("ChunkSpawner"));
	
	// 장애물 매니저 컴포넌트 생성
	ObstacleManager = CreateDefaultSubobject<UExObstacleManager>(TEXT("ObstacleManager"));
}

void AExRunnerGameMode::BeginPlay()
{
	Super::BeginPlay();

	// ========== GameplayTag Event Registration ==========
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

	if (ChunkSpawner)
	{
		ChunkSpawner->InitializeSpawner();
	}

	if (ObstacleManager && ChunkSpawner)
	{
		// ObstacleManager가 ChunkSpawner 이벤트를 구독하도록 초기화
		ObstacleManager->BindToSpawner(ChunkSpawner);
	}

	UE_LOG(LogExRunnerPlay, Log, TEXT("ExRunnerGameMode: Runner Game Started. TreadmillSpeed: %f"), CurrentTreadmillSpeed);
}

void AExRunnerGameMode::StopRunnerGame()
{
	CurrentTreadmillSpeed = 0.f;
	bRunnerModeEnabled = false;
	
	UE_LOG(LogExRunnerPlay, Log, TEXT("ExRunnerGameMode: Runner Game Stopped."));
}

void AExRunnerGameMode::SetTreadmillPaused(bool bPaused)
{
	if (bTreadmillPaused != bPaused)
	{
		bTreadmillPaused = bPaused;
		UE_LOG(LogExRunnerPlay, Log, TEXT("Treadmill Paused: %s (Climbing/Interaction)"), bPaused ? TEXT("True") : TEXT("False"));
	}
}

void AExRunnerGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bRunnerModeEnabled)
	{
		return;
	}

	// [Speed] 시간 경과에 따른 속도 증가
	if (!bTreadmillPaused)
	{
		CurrentTreadmillSpeed += TreadmillAcceleration * DeltaTime;
	}

	// [World Shift] 청크들을 뒤로 이동 (Treadmill)
	if (ChunkSpawner && !bTreadmillPaused)
	{
		ChunkSpawner->ShiftWorld(-CurrentTreadmillSpeed * DeltaTime);
	}

	// [Distance] 거리 누적
	if (!bTreadmillPaused)
	{
		TotalDistance += CurrentTreadmillSpeed * DeltaTime;
	}
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

