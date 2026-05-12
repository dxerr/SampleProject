// Fill out your copyright notice in the Description page of Project Settings.

#include "ExRunnerStatComponent.h"
#include "ExItemTags.h"
#include "Events/ExGameplayEventSubsystem.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "Math/UnrealMathUtility.h"
#include "Util/Actor/ExActorUtil.h"
#include "MoverComponent.h"
#include "../GameStates/ExRunnerGameState.h"

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

	// 아이템 획득 이벤트 라우팅 등록
	if (UWorld* World = GetWorld())
	{
		if (UExGameplayEventSubsystem* EventSub = World->GetSubsystem<UExGameplayEventSubsystem>())
		{
			EventSub->GetEventDelegate(TAG_Ex_Item_PickedUp_Score).AddDynamic(this, &UExRunnerStatComponent::OnScorePickedUp);
		}
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
	// BeginPlay 타이밍 실패 등으로 캐싱이 안 됐을 경우, 폴링 시점마다 재시도합니다.
	if (!CachedMoverComponent.IsValid())
	{
		CachedMoverComponent = BoundPawn->FindComponentByClass<UMoverComponent>();
		if (CachedMoverComponent.IsValid())
		{
			UE_LOG(LogExRunnerStatComp, Log,
				TEXT("[ExRunnerStatComp] MoverComponent 재캐싱 성공 (UpdateStats 시점). 이후 정상 속도 수집 가능."));
		}
	}

	float Speed = 0.0f;
	if (CachedMoverComponent.IsValid())
	{
		Speed = CachedMoverComponent->GetVelocity().Size();
	}
	else
	{
		// MoverComponent를 여전히 못 찾은 경우: APawn::GetVelocity()는 Mover에서 항상 0 반환.
		// 이 경우 UI는 0을 표시하게 되므로, 반복 로그는 최소화합니다.
		Speed = BoundPawn->GetVelocity().Size();
	}
	SetCurrentRunningSpeed(Speed);

	// ─── Distance ──────────────────────────────────────────────────────────────
	if (AExRunnerGameState* GS = GetWorld()->GetGameState<AExRunnerGameState>())
	{
		SetCurrentDistance(GS->CurrentPathDistance);
	}
}

void UExRunnerStatComponent::SetCurrentDistance(float NewDistance)
{
	// 10cm 단위 의미 있는 갱신
	if (!FMath::IsNearlyEqual(CurrentDistance, NewDistance, 10.0f))
	{
		CurrentDistance = NewDistance;
		OnRunnerDistanceChanged.Broadcast(CurrentDistance);
	}
}

void UExRunnerStatComponent::OnScorePickedUp(FGameplayTag Tag, const FExGameplayEventPayload& Payload)
{
	// 이 이벤트를 유발한 대상(아이템을 먹은 캐릭터)이 나 자신인지 확인
	if (Payload.Instigator != BoundPawn.Get() && Payload.Target != BoundPawn.Get())
	{
		return;
	}

	int32 Amount = FMath::RoundToInt32(Payload.OptionalValue);
	AddCoinCount(Amount);
}

void UExRunnerStatComponent::AddCoinCount(int32 Amount)
{
	if (Amount != 0)
	{
		CoinCount += Amount;
		OnCoinCountChanged.Broadcast(CoinCount);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(1, 3.0f, FColor::Cyan,
				FString::Printf(TEXT("코인 획득! 남은 코인: %d"), CoinCount));
		}
	}
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
