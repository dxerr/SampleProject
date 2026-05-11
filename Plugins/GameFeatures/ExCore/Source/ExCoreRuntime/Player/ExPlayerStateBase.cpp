// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/ExPlayerStateBase.h"
#include "Net/UnrealNetwork.h"

AExPlayerStateBase::AExPlayerStateBase()
{
	PreviousScore = 0.0f;
}

void AExPlayerStateBase::AddScore(float Amount)
{
	if (!HasAuthority())
	{
		return;
	}

	float OldScore = GetScore();
	SetScore(OldScore + Amount);

	// 서버도 자체 리슨서버/싱글플레이 클라이언트용으로 OnRep_Score 시뮬레이션을 호출
	OnRep_Score();
}

void AExPlayerStateBase::OnRep_Score()
{
	// 기본 로직 수행 (필요 시)
	Super::OnRep_Score();

	float CurrentScore = GetScore();
	
	// 변경 델리게이트 발송
	if (PreviousScore != CurrentScore)
	{
		OnScoreChangedDelegate.Broadcast(PreviousScore, CurrentScore);
		PreviousScore = CurrentScore;
	}
}

void AExPlayerStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AExPlayerStateBase, bIsMatchReady);
}
