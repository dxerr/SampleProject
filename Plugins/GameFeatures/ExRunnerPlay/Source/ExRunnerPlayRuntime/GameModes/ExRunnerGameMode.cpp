// Copyright ExFrameWork. All Rights Reserved.
// 오프셋 기반 트레드밀 시스템: BaseSpeed + 캐릭터 위치 오프셋 보정

#include "ExRunnerGameMode.h"
#include "../Components/ExRunnerMovementComponent.h"
#include "../Components/ExChunkSpawner.h"
#include "../Components/ExObstacleManager.h"
#include "../Components/ExRunnerItemManager.h"
#include "../Components/ExBeatSyncComponent.h"
#include "../Components/ExPathManager.h"
#include "../Components/ExRunnerRuleManagerComponent.h"
#include "../GameStates/ExRunnerGameState.h"
#include "../Player/ExRunnerPlayerState.h"
#include "../Actors/ExFloorChunk.h"
#include "../Data/ExRunnerConfig.h"
#include "Subsystems/ExDataCenterSubsystem.h"
#include "ExGameplayTags.h"
#include "ExGameplayEventSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Tags/ExMatchTags.h"
#include "Tags/ExRunnerTags.h"
#include "Player/ExPlayerControllerBase.h"
#include "Subsystems/ExMusicManagerSubsystem.h"
#include "Data/ExBGMTrackDataAsset.h"
#include "Components/BoxComponent.h"
#include "ExRunnerPlayRuntimeModule.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h" // 디버그 드로잉용



// ... (Existing Include)

// 캐릭터 조향(UpdateCharacterRotation) 로직은 UExRunnerMovementComponent로 이관되었습니다.


AExRunnerGameMode::AExRunnerGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true; // [Fix] AExGameModeBase에서 꺼진 Tick을 플로어 경로 갱신을 위해 재활성화

	// NOTE: 기본 TickGroup(TG_PrePhysics) 사용
	// 오프셋 기반 보정이므로 X 리셋이 없어 Tick 순서에 민감하지 않음

	BeatSyncComponent = CreateDefaultSubobject<UExBeatSyncComponent>(TEXT("BeatSyncComponent"));
	RuleManagerComponent = CreateDefaultSubobject<UExRunnerRuleManagerComponent>(TEXT("RuleManagerComponent"));
	
	// PathManager는 AExRunnerGameState로 이관되었습니다.
	// [Fix] 블루프린트 생성 시 부모의 게임스테이트가 상속되지 않도록 명시적 기본값 설정
	GameStateClass = AExRunnerGameState::StaticClass();
	PlayerStateClass = AExRunnerPlayerState::StaticClass();
	
	// 매치 준비 완료 시 자동 시작 켜기
	bAutoStartOnReady = true;
}

void AExRunnerGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UExDataCenterSubsystem* DataCenter = GI->GetSubsystem<UExDataCenterSubsystem>())
		{
			if (UExRunnerConfig* Config = DataCenter->GetConfig<UExRunnerConfig>())
			{
				RunnerConfig = Config;
			}
		}
	}
}

void AExRunnerGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 게임 이벤트 시스템(상승/하강, Climb) 구독
	if (UWorld* World = GetWorld())
	{
		if (UExGameplayEventSubsystem* EventSubsystem = World->GetSubsystem<UExGameplayEventSubsystem>())
		{
			EventSubsystem->GetEventDelegate(TAG_Ex_Action_Climb_Start).AddDynamic(this, &AExRunnerGameMode::OnTraversalStart);
			EventSubsystem->GetEventDelegate(TAG_Ex_Action_Climb_End).AddDynamic(this, &AExRunnerGameMode::OnTraversalEnd);

			// 룰 종료 이벤트 구독 — TimeUp/GoalReached 시 SetMatchPhase(PostMatch) 호출
			EventSubsystem->GetEventDelegate(TAG_Rule_TimeUp).AddDynamic(this, &AExRunnerGameMode::OnRuleEndGameEvent);
			EventSubsystem->GetEventDelegate(TAG_Rule_GoalReached).AddDynamic(this, &AExRunnerGameMode::OnRuleEndGameEvent);
		}

		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UExDataCenterSubsystem* DataCenter = GI->GetSubsystem<UExDataCenterSubsystem>())
			{
				if (UExRunnerConfig* Config = DataCenter->GetConfig<UExRunnerConfig>())
				{
					RunnerConfig = Config;
				}
			}
		}

		// 플레이어가 접속하기 전 월드 초기 세팅 완료
		PrewarmRunnerWorld();
	}
}

void AExRunnerGameMode::PrewarmRunnerWorld()
{
	AExRunnerGameState* GS = GetGameState<AExRunnerGameState>();
	if (GS)
	{
		GS->CurrentPathDistance = 0.f;
		GS->RealPlayerPathDistance = 0.f;

		// 랜덤 시드 생성 (데디/리슨 서버 공통)
		GS->SharedTrackSeed = FMath::Rand();
		GS->CurrentSegmentIndex = 0;
		GS->SegmentStartDistance = 0.f;
		GS->CleanupWatermark = 0.f;

		// PathManager 초기화
		if (GS->PathManager)
		{
			if (!RunnerConfig.IsValid())
			{
				if (UGameInstance* GI = GetGameInstance())
				{
					if (UExDataCenterSubsystem* DataCenter = GI->GetSubsystem<UExDataCenterSubsystem>())
					{
						RunnerConfig = DataCenter->GetConfig<UExRunnerConfig>();
					}
				}
			}

			if (RunnerConfig.IsValid())
			{
				GS->PathManager->RunnerConfig = RunnerConfig;
			}
			GS->PathManager->InitializePath(FVector::ZeroVector, FRotator::ZeroRotator);
			UE_LOG(LogExRunnerPlay, Log, TEXT("PathManager 초기화 완료 (RunnerConfig=%s)"),
				RunnerConfig.IsValid() ? *RunnerConfig->GetName() : TEXT("None"));
		}

		// ── 매니저 바인딩 및 시드 초기화 ──
		if (GS->ObstacleManager)
		{
			GS->ObstacleManager->InitializeRandomStream(GS->SharedTrackSeed);
		}
		if (GS->ItemManager)
		{
			GS->ItemManager->InitializeRandomStream(GS->SharedTrackSeed);
		}

		if (GS->ChunkSpawner)
		{
			if (GS->ObstacleManager && GS->ItemManager)
			{
				GS->ChunkSpawner->SetManagers(GS->ObstacleManager, GS->ItemManager);
				GS->ObstacleManager->BindToSpawner(GS->ChunkSpawner);
				GS->ItemManager->BindToSpawner(GS->ChunkSpawner);
			}

			if (RunnerConfig.IsValid())
			{
				GS->ChunkSpawner->RunnerConfig = RunnerConfig;
			}
			GS->ChunkSpawner->InitializeSpawner();
		}

		if (BeatSyncComponent && GS->ObstacleManager)
		{
			BeatSyncComponent->BindToObstacleManager(GS->ObstacleManager);
		}

		UE_LOG(LogExRunnerPlay, Log, TEXT("PrewarmRunnerWorld 완료: 맵 생성 동기화 준비 끝."));
	}
}

void AExRunnerGameMode::StartRunnerGame()
{
	// BGM 시작 (Track Data 적용 시 내부에서 알아서 Phase 믹싱 데이터까지 동기화)
	if (CurrentStageBGM && CurrentStageBGM->BGMAsset)
	{
		if (AExRunnerGameState* RunnerGS = GetGameState<AExRunnerGameState>())
		{
			RunnerGS->SetStageBGM(CurrentStageBGM);
		}
	}

	UE_LOG(LogExRunnerPlay, Log, TEXT("ExRunnerGameMode Started (World Moved Mode)"));
}

void AExRunnerGameMode::StopRunnerGame()
{
	// BGM 정지
	if (UExMusicManagerSubsystem* MusicMgr = GetWorld()->GetSubsystem<UExMusicManagerSubsystem>())
	{
		MusicMgr->StopBGM(1.5f);
	}

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

	// [수정] 멀티플레이어 환경에서 싱글 플레이어 0번 기준의 위치 동기화(Rubber-Banding 버그 원인) 제거 완료.
	// 이제 각 캐릭터의 ExRunnerMovementComponent가 자신의 거리를 계산하고 PlayerState에 동기화하며,
	// GameState가 이를 취합해 LeadDistance/CurrentPathDistance를 관리합니다.

	// 캐릭터 위치/방향 조향 로직은 클라이언트 보간을 위해 MovementComponent로 이관되었습니다.
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

// ──────────────────────────────────────────────
// Traversal Event Callbacks
// ──────────────────────────────────────────────
void AExRunnerGameMode::OnTraversalStart(FGameplayTag EventTag, const FExGameplayEventPayload& Payload)
{
	if (EventTag == TAG_Ex_Action_Climb_Start)
	{
		bIsTraversing = true;
		UE_LOG(LogExRunnerPlay, Log, TEXT("ExRunnerGameMode: Traversal Started - Rotation Override Paused"));
	}
}

void AExRunnerGameMode::OnTraversalEnd(FGameplayTag EventTag, const FExGameplayEventPayload& Payload)
{
	if (EventTag == TAG_Ex_Action_Climb_End)
	{
		bIsTraversing = false;
		UE_LOG(LogExRunnerPlay, Log, TEXT("ExRunnerGameMode: Traversal Ended - Rotation Override Resumed"));
	}
}

void AExRunnerGameMode::OnMatchStarted_Implementation()
{
	Super::OnMatchStarted_Implementation();

	if (bRunnerModeEnabled)
	{
		StartRunnerGame();
	}

	// 룰 시스템 활성화
	if (RuleManagerComponent)
	{
		RuleManagerComponent->ActivateAllRules();
	}
}

void AExRunnerGameMode::OnMatchEnded_Implementation()
{
	// 룰 시스템 비활성화 먼저 (역순 정리)
	if (RuleManagerComponent)
	{
		RuleManagerComponent->DeactivateAllRules();
	}

	if (bRunnerModeEnabled)
	{
		StopRunnerGame();
	}

	Super::OnMatchEnded_Implementation();
}

UShapeComponent* AExRunnerGameMode::SpawnKillVolume(float KillVolumeZ)
{
	// 실제 Kill Volume 스폰은 RuleManagerComponent에 위임
	// (RuleManagerComponent가 Volume의 소유권 및 수명을 관리)
	if (RuleManagerComponent)
	{
		return RuleManagerComponent->SpawnKillVolume(KillVolumeZ);
	}
	return nullptr;
}

// ──────────────────────────────────────────────
// BGM / Phase 연동
// ──────────────────────────────────────────────
void AExRunnerGameMode::SetRunnerPhase(FGameplayTag NewPhase)
{
	if (UExMusicManagerSubsystem* MusicMgr = GetWorld()->GetSubsystem<UExMusicManagerSubsystem>())
	{
		MusicMgr->TransitionToPhase(NewPhase);
	}

	UE_LOG(LogExRunnerPlay, Log, TEXT("러너 Phase 전환: %s"), *NewPhase.ToString());
}

// ──────────────────────────────────────────────
// 룰 종료 이벤트 콜백
// ──────────────────────────────────────────────
void AExRunnerGameMode::OnRuleEndGameEvent(FGameplayTag EventTag, const FExGameplayEventPayload& Payload)
{
	// 서버 권한 검증
	if (!HasAuthority()) return;

	UE_LOG(LogExRunnerPlay, Log, TEXT("[ExRunnerGameMode] 룰 종료 이벤트 수신: %s → Match_PostMatch 전환"), *EventTag.ToString());

	// Match_PostMatch로 전환 → ExGameModeBase::SetMatchPhase가 OnMatchEnded() 호출
	SetMatchPhase(ExMatchTags::Match_PostMatch);
}

void AExRunnerGameMode::CheckAlivePlayers()
{
	if (!HasAuthority()) return;

	int32 AliveCount = 0;
	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController* PC = Iterator->Get();
			if (PC && PC->GetPawn())
			{
				AliveCount++;
			}
		}
	}

	UE_LOG(LogExRunnerPlay, Log, TEXT("[ExRunnerGameMode] CheckAlivePlayers: 남은 생존자 %d 명"), AliveCount);

	if (AliveCount == 0)
	{
		UE_LOG(LogExRunnerPlay, Log, TEXT("[ExRunnerGameMode] 전멸! 매치 종료 처리"));
		
		// 게임 상태를 FallDeath(또는 전멸 이유)로 세팅 (여기서는 우선 FallDeath로 취급)
		if (AExRunnerGameState* GS = GetGameState<AExRunnerGameState>())
		{
			// 개별 룰로 다 죽은 거라면 보통 FallDeath가 마지막 원인
			GS->SetGameOverReason(EExRunnerGameOverReason::FallDeath); 
		}
		
		SetMatchPhase(ExMatchTags::Match_PostMatch);
	}
}

void AExRunnerGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	AExGameStateBase* GS = GetGameState<AExGameStateBase>();
	if (GS && GS->GetCurrentMatchPhase() != ExMatchTags::Match_WaitingForPlayers)
	{
		UE_LOG(LogExRunnerPlay, Warning, TEXT("[ExRunnerGameMode] Late Join blocked for %s"), *NewPlayer->GetName());
		if (AExPlayerControllerBase* ExPC = Cast<AExPlayerControllerBase>(NewPlayer))
		{
			ExPC->Client_ShowLateJoinPopup();
		}
		// 스폰을 스킵하거나 관전자로 변경하는 등의 후속 처리 가능
		return;
	}

	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

AActor* AExRunnerGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	AActor* DefaultStart = Super::ChoosePlayerStart_Implementation(Player);
	if (!DefaultStart)
	{
		ensureMsgf(false, TEXT("PlayerStart not found in map %s"), *GetWorld()->GetName());
	}
	return DefaultStart;
}

APawn* AExRunnerGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	FTransform AdjustedTransform = SpawnTransform;

	if (!RunnerConfig.IsValid())
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UExDataCenterSubsystem* DataCenter = GI->GetSubsystem<UExDataCenterSubsystem>())
			{
				RunnerConfig = DataCenter->GetConfig<UExRunnerConfig>();
			}
		}
	}

	// 4인 이상 매치 시도 차단
	if (RunnerConfig.IsValid() && NextLaneSlotIndex >= RunnerConfig->MatchFlow.LaneSlotOrder.Num())
	{
		ensureMsgf(false, TEXT("4인 이상 매치는 미지원, 가변 레인 시스템 도입 필요"));
		NextLaneSlotIndex = FMath::Max(0, RunnerConfig->MatchFlow.LaneSlotOrder.Num() - 1);
	}

	int32 TargetLaneIndex = 0;
	if (RunnerConfig.IsValid() && RunnerConfig->MatchFlow.LaneSlotOrder.IsValidIndex(NextLaneSlotIndex))
	{
		TargetLaneIndex = RunnerConfig->MatchFlow.LaneSlotOrder[NextLaneSlotIndex];
	}

	// 레인 폭 결정
	float LaneWidth = 100.f;
	TArray<AActor*> FloorChunks;
	UGameplayStatics::GetAllActorsOfClass(this, AExFloorChunk::StaticClass(), FloorChunks);
	if (FloorChunks.Num() > 0)
	{
		if (AExFloorChunk* Chunk = Cast<AExFloorChunk>(FloorChunks[0]))
		{
			LaneWidth = Chunk->GetFloorBounds().GetSize().Y / 3.0f;
		}
	}
	else if (RunnerConfig.IsValid())
	{
		LaneWidth = RunnerConfig->Movement.LaneWidth;
	}

	// 인덱스 증가
	NextLaneSlotIndex++;

	// Transform 계산 (우측 벡터를 기준으로 오프셋 적용)
	// 멀티플레이 스폰 시 여러 PlayerStart 중 하나가 무작위로 선택되더라도, 
	// 항상 트랙의 중앙(레인 0)을 기준으로 배치되도록 기존 좌우 편차를 제거합니다.
	FVector StartLoc = AdjustedTransform.GetLocation();
	FVector RightVec = AdjustedTransform.GetRotation().GetAxisY();
	
	// 투영을 통해 현재 StartLoc의 RightVec 방향 오프셋을 구함
	float CurrentRightOffset = FVector::DotProduct(StartLoc, RightVec);
	
	// 편차를 제거하여 해당 축 기준 0(중앙)으로 정렬
	StartLoc -= RightVec * CurrentRightOffset;
	
	// 타겟 레인 인덱스에 맞는 오프셋을 새로 적용
	StartLoc += RightVec * (TargetLaneIndex * LaneWidth);
	
	AdjustedTransform.SetLocation(StartLoc);

	return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, AdjustedTransform);
}

int32 AExRunnerGameMode::GetExpectedPlayerCount() const
{
	if (RunnerConfig.IsValid())
	{
		return RunnerConfig->MatchFlow.ExpectedPlayerCount;
	}
	return Super::GetExpectedPlayerCount();
}

int32 AExRunnerGameMode::GetCountdownDuration() const
{
	if (RunnerConfig.IsValid())
	{
		return RunnerConfig->MatchFlow.CountdownDurationSeconds;
	}
	return Super::GetCountdownDuration();
}

float AExRunnerGameMode::GetMaxWaitForPlayersSeconds() const
{
	if (RunnerConfig.IsValid())
	{
		return RunnerConfig->MatchFlow.MaxWaitForPlayersSeconds;
	}
	return Super::GetMaxWaitForPlayersSeconds();
}
