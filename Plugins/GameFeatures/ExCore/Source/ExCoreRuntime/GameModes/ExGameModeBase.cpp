// Fill out your copyright notice in the Description page of Project Settings.

#include "GameModes/ExGameModeBase.h"
#include "GameModes/ExGameStateBase.h"
#include "Tags/ExMatchTags.h"
#include "Subsystems/ExGameFlowSubsystem.h"
#include "GameFramework/PlayerController.h"

AExGameModeBase::AExGameModeBase()
{
	// 전이 맵 초기화
	// WaitingForPlayers -> Countdown
	AllowedMatchTransitions.Add(ExMatchTags::Match_WaitingForPlayers, { ExMatchTags::Match_Countdown });
	
	// Countdown -> Playing
	AllowedMatchTransitions.Add(ExMatchTags::Match_Countdown, { ExMatchTags::Match_Playing });
	
	// Playing -> PostMatch
	AllowedMatchTransitions.Add(ExMatchTags::Match_Playing, { ExMatchTags::Match_PostMatch });
	
	// PostMatch -> WaitingForPlayers (재시작)
	AllowedMatchTransitions.Add(ExMatchTags::Match_PostMatch, { ExMatchTags::Match_WaitingForPlayers });
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

	// 상태 전이 유효성 검사
	bool bIsAllowedTransition = false;
	if (bForceTransition)
	{
		bIsAllowedTransition = true;
	}
	else if (const TArray<FGameplayTag>* ValidNextStates = AllowedMatchTransitions.Find(CurrentPhase))
	{
		bIsAllowedTransition = ValidNextStates->Contains(NewPhase);
	}

	if (!bIsAllowedTransition)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ExGameModeBase] Invalid match phase transition from %s to %s"), 
			*CurrentPhase.ToString(), *NewPhase.ToString());
		return;
	}

	FGameplayTag OldPhase = CurrentPhase;
	ExGameState->CurrentMatchPhase = NewPhase; // friend 선언으로 접근 가능
	
	// 서버 자신도 로컬 델리게이트를 돌도록 강제 트리거
	ExGameState->OnRep_MatchPhase(OldPhase);
}

void AExGameModeBase::OnFlowSubsystemRequestTravel(const FString& MapURL)
{
	if (UWorld* World = GetWorld())
	{
		UE_LOG(LogTemp, Log, TEXT("[ExGameModeBase] Performing ServerTravel to URL: %s"), *MapURL);
		World->ServerTravel(MapURL);
	}
}

bool AExGameModeBase::CheckAllPlayersReady() const
{
	if (!GetWorld()) return false;
	
	// 기본적인 예시: 모든 플레이어 컨트롤러를 검사하여 로딩 완료 상태인지 체크합니다.
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
	// 시작 처리 가능
}

AActor* AExGameModeBase::ChoosePlayerStart_Implementation(AController* Player)
{
	return Super::ChoosePlayerStart_Implementation(Player);
}

void AExGameModeBase::OnMatchStarted_Implementation()
{
}

void AExGameModeBase::OnMatchEnded_Implementation()
{
}
