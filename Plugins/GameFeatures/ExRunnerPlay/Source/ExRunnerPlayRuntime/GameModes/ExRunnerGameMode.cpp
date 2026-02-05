// Copyright ExFrameWork. All Rights Reserved.

#include "ExRunnerGameMode.h"
#include "../Components/ExChunkSpawner.h"
#include "../Components/ExObstacleManager.h"

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

	if (bRunnerModeEnabled)
	{
		StartRunnerGame();
	}
}

void AExRunnerGameMode::StartRunnerGame()
{
	CurrentGameSpeed = BaseGameSpeed;
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

	UE_LOG(LogTemp, Log, TEXT("ExRunnerGameMode: Runner Game Started. Speed: %f"), CurrentGameSpeed);
}

void AExRunnerGameMode::StopRunnerGame()
{
	CurrentGameSpeed = 0.f;
	bRunnerModeEnabled = false;
	
	UE_LOG(LogTemp, Log, TEXT("ExRunnerGameMode: Runner Game Stopped."));
}

void AExRunnerGameMode::SetTreadmillPaused(bool bPaused)
{
	if (bTreadmillPaused != bPaused)
	{
		bTreadmillPaused = bPaused;
		UE_LOG(LogTemp, Log, TEXT("Treadmill Paused: %s (Climbing/Interaction)"), bPaused ? TEXT("True") : TEXT("False"));
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
	if (!bTreadmillPaused) // 멈춰있지 않을 때만 가속? 혹은 멈춰도 시간은 가나? 보통 멈추면 가속도 멈춤
	{
		CurrentGameSpeed += SpeedAcceleration * DeltaTime;
	}

	// [World Shift] 청크들을 뒤로 이동 (Treadmill)
	// 일시 정지 상태(등반 중)가 아닐 때만 이동
	if (ChunkSpawner && !bTreadmillPaused)
	{
		ChunkSpawner->ShiftWorld(-CurrentGameSpeed * DeltaTime);
	}

	// [Distance] 거리 누적
	if (!bTreadmillPaused)
	{
		TotalDistance += CurrentGameSpeed * DeltaTime;
	}
}
