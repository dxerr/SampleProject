// Fill out your copyright notice in the Description page of Project Settings.

#include "GameModes/ExGameStateBase.h"
#include "Tags/ExMatchTags.h"
#include "Events/ExGameplayEventSubsystem.h"
#include "Net/UnrealNetwork.h"

AExGameStateBase::AExGameStateBase()
{
	// 기본값은 WaitingForPlayers로 세팅
	CurrentMatchPhase = ExMatchTags::Match_WaitingForPlayers;
	CountdownSecondsRemaining = 0;
}

void AExGameStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AExGameStateBase, CurrentMatchPhase);
	DOREPLIFETIME(AExGameStateBase, CountdownSecondsRemaining);
}

void AExGameStateBase::BeginPlay()
{
	Super::BeginPlay();

	// 난입(Join-In-Progress) 예외 처리
	// 서버에서 이미 특정 Phase로 넘어간 상태에서 클라이언트가 늦게 접속했을 때, 
	// 클라이언트 로컬 변수의 기본값(Waiting)과 서버의 현재 값이 같거나 아예 다를 경우
	// OnRep이 누락될 수 있으므로, 로컬 접속 시 강제로 한 번 최신 값을 기반으로 알림을 보냅니다.
	if (GetLocalRole() < ROLE_Authority)
	{
		// 임시로 OldPhase는 빈 태그로 전송 (첫 초기화임을 암시)
		FGameplayTag EmptyOldPhase;
		OnMatchPhaseChanged.Broadcast(EmptyOldPhase, CurrentMatchPhase);
		HandleMatchPhaseChanged(EmptyOldPhase, CurrentMatchPhase);
	}
}

void AExGameStateBase::SetMatchPhase(FGameplayTag NewPhase, bool bForceTransition /*= false*/)
{
	if (GetLocalRole() != ROLE_Authority)
	{
		return; // 서버 권한에서만 허용
	}

	if (CurrentMatchPhase == NewPhase)
	{
		return;
	}

	FGameplayTag OldPhase = CurrentMatchPhase;
	CurrentMatchPhase = NewPhase;
	
	// 서버 자신도 리플리케이트 콜백을 강제로 로컬 실행
	OnRep_MatchPhase(OldPhase);
}

bool AExGameStateBase::IsMatchActive() const
{
	return CurrentMatchPhase == ExMatchTags::Match_Playing;
}

void AExGameStateBase::SetCountdownSeconds(int32 NewSeconds)
{
	if (GetLocalRole() != ROLE_Authority)
	{
		return; // 서버 권한에서만 허용
	}

	if (CountdownSecondsRemaining == NewSeconds)
	{
		return;
	}

	int32 OldSeconds = CountdownSecondsRemaining;
	CountdownSecondsRemaining = NewSeconds;

	// 서버 자신도 리플리케이트 콜백을 강제로 로컬 실행
	OnRep_CountdownSeconds(OldSeconds);
}

void AExGameStateBase::OnRep_CountdownSeconds(int32 OldSeconds)
{
	// BP/UI로 알림
	OnCountdownChanged.Broadcast(CountdownSecondsRemaining);
}

void AExGameStateBase::OnRep_MatchPhase(const FGameplayTag& OldPhase)
{
	// 하위 클래스에서 필요한 처리 수행
	HandleMatchPhaseChanged(OldPhase, CurrentMatchPhase);

	// BP/UI로 알림
	OnMatchPhaseChanged.Broadcast(OldPhase, CurrentMatchPhase);
	
	UE_LOG(LogTemp, Log, TEXT("[ExGameStateBase] Match Phase Changed: %s -> %s"), 
		*OldPhase.ToString(), *CurrentMatchPhase.ToString());
}

void AExGameStateBase::HandleMatchPhaseChanged(const FGameplayTag& OldPhase, const FGameplayTag& NewPhase)
{
	// 하위 블루프린트나 상속받은 C++ 클래스에서 확장 가능하도록 비워둠
	
	// 매치 페이즈 변경 시, 전역으로 이벤트 브로드캐스트.
	// (Ex: BP_ExPlayerControllerBase 등에서 UI를 트리거 하는 용도로 델리게이트 수신)
	if (UWorld* World = GetWorld())
	{
		if (UExGameplayEventSubsystem* EventSubsystem = World->GetSubsystem<UExGameplayEventSubsystem>())
		{
			EventSubsystem->BroadcastEventSimple(NewPhase, this);
			UE_LOG(LogTemp, Log, TEXT("[ExGameStateBase] 전역 이벤트 발송 완료: %s"), *NewPhase.ToString());
		}
	}
}
