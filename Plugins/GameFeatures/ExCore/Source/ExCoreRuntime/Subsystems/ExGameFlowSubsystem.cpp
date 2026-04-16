// Fill out your copyright notice in the Description page of Project Settings.

#include "Subsystems/ExGameFlowSubsystem.h"
#include "Tags/ExFlowTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogExGameFlow, Log, All);

void UExGameFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 초기 상태 지정
	CurrentFlowState = ExFlowTags::Flow_Boot;

	// 허용되는 상태 전이 맵 (Transition Map) 구성
	// Boot -> Auth.IDP (정식 인증 경로) 또는 Lobby (개발/인증 우회 경로)
	AllowedTransitions.Add(ExFlowTags::Flow_Boot, { ExFlowTags::Flow_Auth_IDP, ExFlowTags::Flow_Lobby });
	
	// Auth.IDP -> Lobby 또는 Boot(재시도)
	AllowedTransitions.Add(ExFlowTags::Flow_Auth_IDP, { ExFlowTags::Flow_Lobby, ExFlowTags::Flow_Boot });
	
	// Lobby -> InGame 또는 Auth.IDP(로그아웃)
	AllowedTransitions.Add(ExFlowTags::Flow_Lobby, { ExFlowTags::Flow_InGame, ExFlowTags::Flow_Auth_IDP });
	
	// InGame -> Lobby(복귀)
	AllowedTransitions.Add(ExFlowTags::Flow_InGame, { ExFlowTags::Flow_Lobby });
}

void UExGameFlowSubsystem::Deinitialize()
{
	// 델리게이트 바인딩 해제
	OnFlowStateChanged.Clear();
	OnRequestTravel.Clear();

	Super::Deinitialize();
}

void UExGameFlowSubsystem::SetFlowState(FGameplayTag NewState)
{
	if (CurrentFlowState == NewState)
	{
		return;
	}

	// 상태 전이 유효성 검사
	bool bIsAllowedTransition = false;
	if (const TArray<FGameplayTag>* ValidNextStates = AllowedTransitions.Find(CurrentFlowState))
	{
		bIsAllowedTransition = ValidNextStates->Contains(NewState);
	}

	if (!bIsAllowedTransition)
	{
		UE_LOG(LogExGameFlow, Warning, TEXT("[ExGameFlowSubsystem] Invalid transition attempted from %s to %s"), 
			*CurrentFlowState.ToString(), *NewState.ToString());
		return;
	}

	FGameplayTag OldState = CurrentFlowState;
	CurrentFlowState = NewState;

	// 변경 델리게이트 브로드캐스트
	OnFlowStateChanged.Broadcast(OldState, CurrentFlowState);

	UE_LOG(LogExGameFlow, Log, TEXT("[ExGameFlowSubsystem] Flow State Changed: %s -> %s"), 
		*OldState.ToString(), *CurrentFlowState.ToString());
}

void UExGameFlowSubsystem::RequestTravel(const FString& MapURL)
{
	// GameMode 등 델리게이트를 수신할 수 있는 체계로 알림
	UE_LOG(LogExGameFlow, Log, TEXT("[ExGameFlowSubsystem] Requesting Travel to %s"), *MapURL);
	OnRequestTravel.Broadcast(MapURL);
}

void UExGameFlowSubsystem::TransitionToInGame(const FString& MapURL)
{
	// 로비 -> 인게임 상태로 내부 전환 시도
	SetFlowState(ExFlowTags::Flow_InGame);

	// 게임 모드에 맵 로딩 지시
	RequestTravel(MapURL);
}
