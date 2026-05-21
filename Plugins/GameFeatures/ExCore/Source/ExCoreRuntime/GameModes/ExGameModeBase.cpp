// Fill out your copyright notice in the Description page of Project Settings.

#include "GameModes/ExGameModeBase.h"
#include "GameModes/ExGameStateBase.h"
#include "GameModes/ExGameSession.h"
#include "Tags/ExMatchTags.h"
#include "Subsystems/ExGameFlowSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "../Data/ExCoreSpawnDataAsset.h"
#include "Experience/ExExperienceManagerComponent.h"
#include "Experience/ExExperienceDefinition.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Components/ExVisualOverrideComponent.h"
#include "Player/ExPlayerStateBase.h"
#include "Player/ExPlayerControllerBase.h"
#include "TimerManager.h"
#include "Engine/LocalPlayer.h"
#include "UI/Subsystems/ExUIManagerSubsystem.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogExCoreGM, Log, All);

AExGameModeBase::AExGameModeBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // [Fix] 불필요한 빈 Tick 활성화 방지

	// Seamless Travel 사용 (주인님 요청사항)
	bUseSeamlessTravel = true;
	
	// 매치 시작 자동화 여부 기본값 
	bAutoStartOnReady = false;

	// 주인님, 자동 로그인 오동작으로 인한 세션 파괴 문제를 프로젝트 레벨에서 우회하기 위해 커스텀 게임 세션 클래스로 지정합니다.
	GameSessionClass = AExGameSession::StaticClass();
}

void AExGameModeBase::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// URL 옵션에서 ExpectedPlayers 파싱
	int32 ParsedCount = UGameplayStatics::GetIntOption(Options, TEXT("ExpectedPlayers"), -1);
	if (ParsedCount > 0)
	{
		DynamicExpectedPlayerCount = ParsedCount;
		UE_LOG(LogExCoreGM, Log, TEXT("[ExGameModeBase] URL 옵션 파싱 완료 — ExpectedPlayers=%d (주인님, 동적 플레이어 수를 정상 반영합니다!)"), DynamicExpectedPlayerCount);
	}
}

void AExGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	// 전역 앱 플로우(GameFlowSubsystem)의 Travel Request 등록
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UExGameFlowSubsystem* FlowSubsystem = GI->GetSubsystem<UExGameFlowSubsystem>())
		{
			FlowSubsystem->OnRequestTravel.AddDynamic(this, &AExGameModeBase::OnFlowSubsystemRequestTravel);
		}
	}

	// GameState의 ExperienceManager에 할당된 DataAsset 주입
	if (DefaultExperience)
	{
		if (AGameStateBase* GS = GetGameState<AGameStateBase>())
		{
			if (UExExperienceManagerComponent* ExpManager = GS->GetComponentByClass<UExExperienceManagerComponent>())
			{
				ExpManager->ServerSetCurrentExperience(DefaultExperience);
				UE_LOG(LogExCoreGM, Log, TEXT("[ExGameModeBase] DefaultExperience (%s) automatically injected to Manager."), *DefaultExperience->GetName());
			}
		}
	}

	UE_LOG(LogExCoreGM, Log, TEXT("[ExGameModeBase] BeginPlay - SpawnDataAsset: %s"), 
		SpawnDataAsset ? *SpawnDataAsset->GetName() : TEXT("None"));

	// 호스트인 경우, 맵 로드가 끝났음을 세션에 알립니다.
	if (HasAuthority())
	{
		if (IOnlineSubsystem* OSS = IOnlineSubsystem::Get(FName(TEXT("EOS"))))
		{
			if (IOnlineSessionPtr SessionInterface = OSS->GetSessionInterface())
			{
				FName SessionName(TEXT("ExMatch"));
				if (FNamedOnlineSession* Session = SessionInterface->GetNamedSession(SessionName))
				{
					FOnlineSessionSettings* Settings = &Session->SessionSettings;
					Settings->Set(FName(TEXT("HOST_MAP_READY")), true, EOnlineDataAdvertisementType::ViaOnlineService);
					SessionInterface->UpdateSession(SessionName, *Settings, true);
					
					FString CurrentNetDriverName = GetWorld()->GetNetDriver() ? GetWorld()->GetNetDriver()->NetDriverName.ToString() : TEXT("None");
					UE_LOG(LogExCoreGM, Log, TEXT("[ExGameModeBase] BeginPlay 진단 로그 - HOST_MAP_READY=true 갱신 완료. 현재 NetDriver: %s, 맵: %s"), *CurrentNetDriverName, *GetWorld()->GetMapName());
				}
			}
		}
	}
}

void AExGameModeBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UExGameFlowSubsystem* FlowSubsystem = GI->GetSubsystem<UExGameFlowSubsystem>())
		{
			FlowSubsystem->OnRequestTravel.RemoveDynamic(this, &AExGameModeBase::OnFlowSubsystemRequestTravel);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AExGameModeBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AExGameModeBase::SetMatchPhase(FGameplayTag NewPhase, bool bForceTransition)
{
	AExGameStateBase* ExGameState = GetGameState<AExGameStateBase>();
	if (!ExGameState)
	{
		return;
	}

	FGameplayTag CurrentPhase = ExGameState->GetCurrentMatchPhase();
	if (CurrentPhase == NewPhase)
	{
		return;
	}

	// [주인님 요청] 상태 전이 유효성 검사 제거 - 모든 상태 전이를 자유롭게 허용
	// (기존 AllowedMatchTransitions 맵 기반 검사 삭제)
	UE_LOG(LogExCoreGM, Log, TEXT("[ExGameModeBase] MatchPhase Transition Requested: %s -> %s"), *CurrentPhase.ToString(), *NewPhase.ToString());

	FGameplayTag OldPhase = CurrentPhase;
	ExGameState->CurrentMatchPhase = NewPhase; // friend 선언으로 접근 가능
	
	// 대기 시간 타임아웃 세팅
	if (NewPhase == ExMatchTags::Match_WaitingForPlayers)
	{
		float WaitTime = GetMaxWaitForPlayersSeconds();
		if (WaitTime > 0.f)
		{
			GetWorld()->GetTimerManager().SetTimer(WaitForPlayersTimerHandle, this, &AExGameModeBase::OnWaitForPlayersTimeout, WaitTime, false);
		}
	}

	// 서버 자신도 로컬 델리게이트를 돌도록 강제 트리거
	ExGameState->OnRep_MatchPhase(OldPhase);

	// 매치 시작/종료 게임모드 콜백
	if (NewPhase == ExMatchTags::Match_Playing && OldPhase != ExMatchTags::Match_Playing)
	{
		OnMatchStarted();
	}
	else if (NewPhase == ExMatchTags::Match_PostMatch && OldPhase != ExMatchTags::Match_PostMatch)
	{
		OnMatchEnded();
	}
}

void AExGameModeBase::OnFlowSubsystemRequestTravel(const FString& MapURL)
{
	if (UWorld* World = GetWorld())
	{
		// [Fix] Seamless Travel은 반드시 TransitionMap이 설정되어야 합니다.
		// TransitionMap 미설정 시 Non-Seamless로 폴백되면서 클라이언트가 같은 포트에
		// 중복 연결을 시도하여 "Host closed the connection" 에러 발생.
		// 안정적인 전환을 위해 Non-Seamless Travel(bSeamless=false)을 사용합니다.
		// (Seamless Travel이 필요한 경우 ProjectSettings에서 TransitionMap을 설정하세요.)
		const bool bSeamless = false;
		UE_LOG(LogExCoreGM, Log, TEXT("[ExGameModeBase] Performing ServerTravel to URL: %s (Non-Seamless)"), *MapURL);
		World->ServerTravel(MapURL, false, bSeamless);
	}
}

void AExGameModeBase::OnPlayerReady(APlayerController* PC)
{
	if (!PC) return;

	AExPlayerStateBase* PS = PC->GetPlayerState<AExPlayerStateBase>();
	if (PS)
	{
		PS->bIsMatchReady = true;
		UE_LOG(LogExCoreGM, Log, TEXT("[ExGameModeBase] OnPlayerReady: %s is marked ready."), *PC->GetName());

		// [ExRunnerStartDiag] bIsMatchReady 설정 확인 로그
		UE_LOG(LogExCoreGM, Log,
			TEXT("[ExRunnerStartDiag] OnPlayerReady | PC='%s' | PS='%s' | bIsMatchReady=%s"),
			*PC->GetName(),
			*PS->GetName(),
			PS->bIsMatchReady ? TEXT("true") : TEXT("false"));

		CheckAndStartMatch();
	}
	else
	{
		UE_LOG(LogExCoreGM, Warning,
			TEXT("[ExRunnerStartDiag] OnPlayerReady | PC='%s' | PlayerState가 null! Ready 설정 실패."),
			*PC->GetName());
	}
}

int32 AExGameModeBase::GetExpectedPlayerCount() const
{
	if (DynamicExpectedPlayerCount > 0)
	{
		return DynamicExpectedPlayerCount;
	}
	// Default to 1 for basic games, can be overridden by specific GameModes
	return 1;
}

int32 AExGameModeBase::GetCountdownDuration() const
{
	return 3;
}

float AExGameModeBase::GetMaxWaitForPlayersSeconds() const
{
	return 30.f;
}

void AExGameModeBase::CheckAndStartMatch()
{
	AExGameStateBase* ExGameState = GetGameState<AExGameStateBase>();
	if (!ExGameState)
	{
		return;
	}

	if (ExGameState->GetCurrentMatchPhase() == ExMatchTags::Match_WaitingForPlayers)
	{
		int32 ReadyPlayers = 0;
		int32 LoadedPlayers = 0;
		int32 TotalPlayers = 0;

		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (PC)
			{
				TotalPlayers++;
				if (PC->HasClientLoadedCurrentWorld())
				{
					LoadedPlayers++;
				}

				if (AExPlayerStateBase* PS = PC->GetPlayerState<AExPlayerStateBase>())
				{
					if (PS->bIsMatchReady)
					{
						ReadyPlayers++;
					}
				}
			}
		}

		bool bAllLoaded = (LoadedPlayers == TotalPlayers) && (TotalPlayers >= GetExpectedPlayerCount());
		bool bAllReady = (ReadyPlayers == TotalPlayers) && (TotalPlayers >= GetExpectedPlayerCount());

		// [ExRunnerStartDiag] 4-AND 조건 현황 로그 (매 CheckAndStartMatch 호출 시 출력)
		UE_LOG(LogExCoreGM, Log,
			TEXT("[ExRunnerStartDiag] CheckAndStartMatch | Total=%d | Loaded=%d | Ready=%d | Expected=%d | bAllLoaded=%s | bAllReady=%s"),
			TotalPlayers, LoadedPlayers, ReadyPlayers, GetExpectedPlayerCount(),
			bAllLoaded ? TEXT("true") : TEXT("false"),
			bAllReady ? TEXT("true") : TEXT("false"));

		if (bAllLoaded && bAllReady)
		{
			UE_LOG(LogExCoreGM, Log, TEXT("[ExRunnerStartDiag] CheckAndStartMatch: 모든 조건 충족! 실제 시작 진행."));
			GetWorld()->GetTimerManager().ClearTimer(WaitForPlayersTimerHandle);
			OnAllPlayersReady();
		}
	}
	else
	{
		// [ExRunnerStartDiag] WaitingForPlayers가 아닌 다른 Phase에서 CheckAndStartMatch 호출 시 로그
		UE_LOG(LogExCoreGM, Log,
			TEXT("[ExRunnerStartDiag] CheckAndStartMatch 무시: CurrentPhase='%s' (이미 WaitingForPlayers 단계 알님)"),
			*ExGameState->GetCurrentMatchPhase().ToString());
	}
}

bool AExGameModeBase::CheckAllPlayersReady() const
{
	if (!GetWorld()) return false;
	
	// 실제 구현에서는 AExPlayerControllerBase 등을 캐싱하여 Pawn의 Possess 완료 여부 등을 검증합니다.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC || !PC->HasClientLoadedCurrentWorld())
		{
			return false;
		}
	}
	return true;
}

void AExGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	// 접속 기록 등 커스텀 로직 수행 가능
}

void AExGameModeBase::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	
	// [Fix] 입장 시 무조건 Playing으로 전환하지 않고, CheckAndStartMatch 의도에 맡깁니다.
	CheckAndStartMatch();
}

AActor* AExGameModeBase::ChoosePlayerStart_Implementation(AController* Player)
{
	return Super::ChoosePlayerStart_Implementation(Player);
}

APawn* AExGameModeBase::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogExCoreGM, Error, TEXT("[ExGameModeBase] SpawnDefaultPawnAtTransform: World is null"));
		return nullptr;
	}

	// 1. 스폰할 Pawn 클래스 결정
	TSubclassOf<APawn> PawnClassToSpawn = nullptr;
	
	if (SpawnDataAsset)
	{
		PawnClassToSpawn = SpawnDataAsset->GetSelectedPawnClass();
	}
	
	// DataAsset에 설정이 없으면 DefaultPawnClass 사용
	if (!PawnClassToSpawn)
	{
		PawnClassToSpawn = DefaultPawnClass;
	}

	if (!PawnClassToSpawn)
	{
		UE_LOG(LogExCoreGM, Warning, TEXT("[ExGameModeBase] SpawnDefaultPawnAtTransform: No valid PawnClass to spawn"));
		return nullptr;
	}

	UE_LOG(LogExCoreGM, Log, TEXT("[ExGameModeBase] Spawning Pawn: %s at %s"), 
		*PawnClassToSpawn->GetName(), *SpawnTransform.ToString());

	// 2. 컨테이너 폰 스폰
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	SpawnParams.Owner = this;

	APawn* SpawnedPawn = World->SpawnActor<APawn>(
		PawnClassToSpawn, 
		SpawnTransform.GetLocation(), 
		SpawnTransform.Rotator(), 
		SpawnParams
	);

	if (!SpawnedPawn)
	{
		UE_LOG(LogExCoreGM, Error, TEXT("[ExGameModeBase] Failed to spawn Pawn"));
		return nullptr;
	}

	// 3. Visual Override 적용 (컴포넌트로 위임)
	if (SpawnDataAsset)
	{
		TSubclassOf<AActor> VisualClass = SpawnDataAsset->GetSelectedVisualOverride();
		if (VisualClass)
		{
			UExVisualOverrideComponent* VisualComp = SpawnedPawn->FindComponentByClass<UExVisualOverrideComponent>();
			if (!VisualComp)
			{
				UE_LOG(LogExCoreGM, Warning, TEXT("[ExGameModeBase] SpawnedPawn %s lacks UExVisualOverrideComponent! Dynamically adding it."), *SpawnedPawn->GetName());
				VisualComp = NewObject<UExVisualOverrideComponent>(SpawnedPawn, TEXT("ExVisualOverrideDynComp"));
				if (VisualComp)
				{
					VisualComp->SetIsReplicated(true);
					VisualComp->RegisterComponent();
					SpawnedPawn->AddInstanceComponent(VisualComp);
					
					// Force Net Update on the owner so the new component replicates immediately
					SpawnedPawn->ForceNetUpdate();
				}
			}

			if (VisualComp)
			{
				VisualComp->SetVisualOverride(
					VisualClass,
					SpawnDataAsset->bHideContainerMesh,
					SpawnDataAsset->bCopyAnimationFromVisual
				);
			}
			else
			{
				UE_LOG(LogExCoreGM, Error, TEXT("[ExGameModeBase] Failed to dynamically create UExVisualOverrideComponent on %s!"), *SpawnedPawn->GetName());
			}
		}
	}

	UE_LOG(LogExCoreGM, Log, TEXT("[ExGameModeBase] Successfully spawned: %s"), *SpawnedPawn->GetName());
	return SpawnedPawn;
}




void AExGameModeBase::ChangeVisualOverride(APawn* TargetPawn, int32 NewVisualIndex)
{
	if (!TargetPawn || !SpawnDataAsset)
	{
		return;
	}

	// 인덱스 유효성 검사
	if (!SpawnDataAsset->VisualOverrides.IsValidIndex(NewVisualIndex))
	{
		UE_LOG(LogExCoreGM, Warning, TEXT("[ExGameModeBase] ChangeVisualOverride: Invalid index %d"), NewVisualIndex);
		return;
	}

	SpawnDataAsset->SelectedVisualIndex = NewVisualIndex;
	TSubclassOf<AActor> NewVisualClass = SpawnDataAsset->VisualOverrides[NewVisualIndex];
	
	UExVisualOverrideComponent* VisualComp = TargetPawn->FindComponentByClass<UExVisualOverrideComponent>();
	if (VisualComp)
	{
		VisualComp->SetVisualOverride(
			NewVisualClass,
			SpawnDataAsset->bHideContainerMesh,
			SpawnDataAsset->bCopyAnimationFromVisual
		);
	}
}

void AExGameModeBase::OnMatchStarted_Implementation()
{
}

void AExGameModeBase::OnMatchEnded_Implementation()
{
}

void AExGameModeBase::OnAllPlayersReady()
{
	StartCountdown();
}

void AExGameModeBase::StartCountdown()
{
	if (AExGameStateBase* ExGameState = GetGameState<AExGameStateBase>())
	{
		ExGameState->SetCountdownSeconds(GetCountdownDuration());
		SetMatchPhase(ExMatchTags::Match_Countdown);
		
		GetWorld()->GetTimerManager().SetTimer(CountdownTimerHandle, this, &AExGameModeBase::FinishCountdown, GetCountdownDuration(), false);
	}
}

void AExGameModeBase::OnWaitForPlayersTimeout()
{
	UE_LOG(LogExCoreGM, Warning, TEXT("[ExGameModeBase] OnWaitForPlayersTimeout: Max wait time reached. Forcing game start."));
	
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UExUIManagerSubsystem* UIManager = LP->GetSubsystem<UExUIManagerSubsystem>())
			{
				UIManager->ShowToast(FText::FromString(TEXT("접속 대기 시간 초과로 강제 시작합니다.")));
			}
		}
	}
	
	OnAllPlayersReady();
}

void AExGameModeBase::FinishCountdown()
{
	GetWorld()->GetTimerManager().ClearTimer(CountdownTimerHandle);
	SetMatchPhase(ExMatchTags::Match_Playing);
}


