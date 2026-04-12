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
#include "../Data/ExCurveConfig.h"
#include "ExGameplayTags.h"
#include "ExGameplayEventSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Tags/ExMatchTags.h"
#include "Tags/ExRunnerTags.h"
#include "Subsystems/ExMusicManagerSubsystem.h"
#include "Data/ExBGMTrackDataAsset.h"
#include "Components/BoxComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h" // 디버그 드로잉용

DEFINE_LOG_CATEGORY_STATIC(LogExRunnerPlay, Log, All);

// ... (Existing Include)

// 캐릭터 조향(UpdateCharacterRotation) 로직은 UExRunnerMovementComponent로 이관되었습니다.


AExRunnerGameMode::AExRunnerGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true; // [Fix] AExGameModeBase에서 꺼진 Tick을 플로어 경로 갱신을 위해 재활성화

	// NOTE: 기본 TickGroup(TG_PrePhysics) 사용
	// 오프셋 기반 보정이므로 X 리셋이 없어 Tick 순서에 민감하지 않음

	ChunkSpawner = CreateDefaultSubobject<UExChunkSpawner>(TEXT("ChunkSpawner"));
	ObstacleManager = CreateDefaultSubobject<UExObstacleManager>(TEXT("ObstacleManager"));
	ItemManager = CreateDefaultSubobject<UExRunnerItemManager>(TEXT("ItemManager"));
	BeatSyncComponent = CreateDefaultSubobject<UExBeatSyncComponent>(TEXT("BeatSyncComponent"));
	RuleManagerComponent = CreateDefaultSubobject<UExRunnerRuleManagerComponent>(TEXT("RuleManagerComponent"));
	
	// PathManager는 AExRunnerGameState로 이관되었습니다.
	// [Fix] 블루프린트 생성 시 부모의 게임스테이트가 상속되지 않도록 명시적 기본값 설정
	GameStateClass = AExRunnerGameState::StaticClass();
	
	// 매치 준비 완료 시 자동 시작 켜기
	bAutoStartOnReady = true;
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
	}
}

void AExRunnerGameMode::StartRunnerGame()
{
	AExRunnerGameState* GS = GetGameState<AExRunnerGameState>();
	if (GS)
	{
		GS->CurrentPathDistance = 0.f;
		GS->RealPlayerPathDistance = 0.f;

		// PathManager 초기화
		if (GS->PathManager)
		{
			if (CurveConfig)
			{
				GS->PathManager->CurveConfig = CurveConfig;
			}
			GS->PathManager->InitializePath(FVector::ZeroVector, FRotator::ZeroRotator);
			UE_LOG(LogExRunnerPlay, Log, TEXT("PathManager 초기화 완료 (CurveConfig=%s)"),
				CurveConfig ? *CurveConfig->GetName() : TEXT("None"));
		}
	}

	if (ChunkSpawner)
	{
		ChunkSpawner->InitializeSpawner();
	}

	if (ObstacleManager && ItemManager && ChunkSpawner)
	{
		ChunkSpawner->SetManagers(ObstacleManager, ItemManager);
		
		// 장애물 매니저의 내부 참조(BoundSpawner) 및 OnChunkDespawned 이벤트 연결을 위해 호출 필수
		ObstacleManager->BindToSpawner(ChunkSpawner);

		// 아이템 매니저도 OnChunkDespawned 이벤트를 받아 액터를 풀에 반환/파괴할 수 있도록 연결
		ItemManager->BindToSpawner(ChunkSpawner);
	}

	if (BeatSyncComponent && ObstacleManager)
	{
		BeatSyncComponent->BindToObstacleManager(ObstacleManager);
	}

	// BGM 시작 (Track Data 적용 시 내부에서 알아서 Phase 믹싱 데이터까지 동기화)
	if (CurrentStageBGM && CurrentStageBGM->BGMAsset)
	{
		if (UExMusicManagerSubsystem* MusicMgr = GetWorld()->GetSubsystem<UExMusicManagerSubsystem>())
		{
			MusicMgr->StartBGM(CurrentStageBGM);
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

	// 핵심 갱신: 플레이어 폰 가져오기
	APawn* PlayerPawn = GetCachedPlayerPawn();
	if (!PlayerPawn)
	{
		return;
	}

	// 실제 이동한 거리를 RealPlayerPathDistance에 갱신
	// 플레이어 위치를 기준으로 가장 가까운 경로 상의 거리(투영)를 계산하여 추적합니다.
	AExRunnerGameState* GS = GetGameState<AExRunnerGameState>();
	if (GS && GS->PathManager)
	{
		FVector PlayerLocation = PlayerPawn->GetActorLocation();
		GS->RealPlayerPathDistance = GS->PathManager->GetClosestDistanceAtLocation(PlayerLocation, GS->RealPlayerPathDistance, 2000.f);
		
		// 가상 경로 거리도 동기화 (트레드밀/LookAhead 참조 등에 사용)
		GS->CurrentPathDistance = GS->RealPlayerPathDistance;
	}

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
