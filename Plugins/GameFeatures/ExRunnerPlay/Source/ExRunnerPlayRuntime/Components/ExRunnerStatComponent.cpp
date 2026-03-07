// Fill out your copyright notice in the Description page of Project Settings.

#include "ExRunnerStatComponent.h"
#include "GameFramework/Pawn.h"
#include "Math/UnrealMathUtility.h"
#include "Util/Actor/ExActorUtil.h"
#include "MoverComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogExRunnerStatComp, Log, All);

UExRunnerStatComponent::UExRunnerStatComponent()
{
	// 순수 데이터 모델이므로 Tick 갱신 오버헤드를 원천 차단합니다.
	// 대신 타이머(StatPollInterval)를 사용하여 주기적으로 스탯을 수집합니다.
	PrimaryComponentTick.bCanEverTick = false;
}

void UExRunnerStatComponent::BeginPlay()
{
	Super::BeginPlay();

	// ExActorUtil을 사용하여 Owner → AttachParent 순으로 Pawn 탐색
	APawn* FoundPawn = UExActorUtil::FindOwnerPawn(this);

	if (!FoundPawn)
	{
		UE_LOG(LogExRunnerStatComp, Warning, TEXT("[ExRunnerStatComponent] Pawn을 찾을 수 없습니다. 스탯 폴링을 시작할 수 없습니다."));
		return;
	}

	BoundPawn = FoundPawn;

	// APawn::GetVelocity()는 Mover 시스템에서 0을 반환하므로 MoverComponent를 직접 캐싱합니다.
	CachedMoverComponent = FoundPawn->FindComponentByClass<UMoverComponent>();
	if (!CachedMoverComponent.IsValid())
	{
		UE_LOG(LogExRunnerStatComp, Warning, TEXT("[ExRunnerStatComponent] MoverComponent를 찾을 수 없습니다. APawn::GetVelocity()로 폴백합니다."));
	}

	// StatPollInterval 주기로 UpdateStats를 반복 호출하는 타이머 등록
	GetWorld()->GetTimerManager().SetTimer(
		StatPollTimerHandle,
		this,
		&UExRunnerStatComponent::UpdateStats,
		StatPollInterval,
		true  // bLoop = true
	);

	UE_LOG(LogExRunnerStatComp, Log, TEXT("[ExRunnerStatComponent] Pawn '%s'에 스탯 폴링 시작 (%.2f초 주기, MoverComponent: %s)"),
		*FoundPawn->GetName(), StatPollInterval,
		CachedMoverComponent.IsValid() ? TEXT("Found") : TEXT("Not Found - Fallback"));
}

void UExRunnerStatComponent::UpdateStats()
{
	if (!BoundPawn.IsValid()) return;

	// ─── Speed ─────────────────────────────────────────────────────────────────
	// Mover 시스템: UMoverComponent::GetVelocity() 사용 (APawn::GetVelocity()는 항상 0 반환)
	// Fallback: MoverComponent가 없으면 기존 APawn::GetVelocity() 사용
	float Speed = 0.0f;
	if (CachedMoverComponent.IsValid())
	{
		Speed = CachedMoverComponent->GetVelocity().Size();
	}
	else
	{
		Speed = BoundPawn->GetVelocity().Size();
	}
	SetCurrentRunningSpeed(Speed);

	// ─── 추후 확장 예시 ─────────────────────────────────────────────────────────
	// SetCurrentDistance(PathManager->GetPlayerDistance());
	// SetCurrentCoinCount(CoinComponent->GetCollectedCoins());
}

void UExRunnerStatComponent::SetCurrentRunningSpeed(float NewSpeed)
{
	// 미세한 소수점 오차로 인한 무의미한 UI 갱신(Broadcast)을 방지하기 위해 1.0f 단위 변화만 감지
	if (!FMath::IsNearlyEqual(CurrentRunningSpeed, NewSpeed, 1.0f))
	{
		CurrentRunningSpeed = NewSpeed;
		OnRunnerSpeedChanged.Broadcast(CurrentRunningSpeed);
	}
}
