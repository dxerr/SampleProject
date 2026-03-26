// Fill out your copyright notice in the Description page of Project Settings.

#include "ExRunnerStatComponent.h"
#include "ExRunnerInputComponent.h"
#include "ExItemTags.h"
#include "ExGameplayEventSubsystem.h"
#include "Engine/Engine.h" // [추가] GEngine 디버그 로그용
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

	// [확장] 스프린트 활성화/비활성화 제어를 위해 InputComponent 캐싱
	CachedInputComponent = FoundPawn->FindComponentByClass<UExRunnerInputComponent>();
	if (!CachedInputComponent.IsValid())
	{
		UE_LOG(LogExRunnerStatComp, Warning, TEXT("[ExRunnerStatComponent] ExRunnerInputComponent를 찾을 수 없습니다. 스프린트 제어가 불가능합니다."));
	}

	// [확장] 아이템 획득 이벤트 라우팅 등록 
	if (UWorld* World = GetWorld())
	{
		if (UExGameplayEventSubsystem* EventSub = World->GetSubsystem<UExGameplayEventSubsystem>())
		{
			EventSub->GetEventDelegate(TAG_Ex_Item_PickedUp_Score).AddDynamic(this, &UExRunnerStatComponent::OnScorePickedUp);
			EventSub->GetEventDelegate(TAG_Ex_Buff_SpeedUp).AddDynamic(this, &UExRunnerStatComponent::OnSpeedUpBuff);
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
	
	UpdateSprintTimer();
}

void UExRunnerStatComponent::UpdateSprintTimer()
{
	if (SprintRemainingTime > 0.0f)
	{
		SprintRemainingTime -= StatPollInterval;
		if (SprintRemainingTime <= 0.0f)
		{
			SprintRemainingTime = 0.0f;
			
			// 스프린트 종료
			if (CachedInputComponent.IsValid())
			{
				CachedInputComponent->RequestSprintAction(false);
			}
			UE_LOG(LogExRunnerStatComp, Log, TEXT("[ExRunnerStatComponent] 스프린트 종료 (타이머 만료)"));
			
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("스프린트 버프 해제됨!"));
			}
		}
		
		// 매 틱마다 잔여시간 UI에 갱신
		OnSprintTimeChanged.Broadcast(SprintRemainingTime);
		
		if (SprintRemainingTime > 0.0f && GEngine)
		{
			// 매 틱 로그 도배 방지용 고정 키(2)를 사용하여 텍스트 라인 재사용
			GEngine->AddOnScreenDebugMessage(2, 0.5f, FColor::Yellow, FString::Printf(TEXT("남은 스프린트 시간: %.1f초"), SprintRemainingTime));
		}
	}
}

void UExRunnerStatComponent::OnScorePickedUp(FGameplayTag Tag, const FExGameplayEventPayload& Payload)
{
	int32 Amount = FMath::RoundToInt32(Payload.OptionalValue);
	AddCoinCount(Amount);
}

void UExRunnerStatComponent::OnSpeedUpBuff(FGameplayTag Tag, const FExGameplayEventPayload& Payload)
{
	ActivateSprint(Payload.Duration);
}

void UExRunnerStatComponent::AddCoinCount(int32 Amount)
{
	if (Amount != 0)
	{
		CoinCount += Amount;
		OnCoinCountChanged.Broadcast(CoinCount);
		// UE_LOG(LogExRunnerStatComp, Verbose, TEXT("[ExRunnerStatComponent] 코인 획득: %d (누적: %d)"), Amount, CoinCount);
		
		if (GEngine)
		{
			// 1번 키 슬롯 덮어쓰기로 화면 도배 방지
			GEngine->AddOnScreenDebugMessage(1, 3.0f, FColor::Cyan, FString::Printf(TEXT("코인 획득! 남은 코인: %d"), CoinCount));
		}
	}
}

void UExRunnerStatComponent::ActivateSprint(float Duration)
{
	if (Duration <= 0.0f) return;

	SprintRemainingTime = Duration; // 남은 시간 갱신/초기화

	if (CachedInputComponent.IsValid())
	{
		CachedInputComponent->RequestSprintAction(true);
	}

	OnSprintTimeChanged.Broadcast(SprintRemainingTime);
	UE_LOG(LogExRunnerStatComp, Log, TEXT("[ExRunnerStatComponent] 스프린트 등반 (%.1f초)"), Duration);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, FString::Printf(TEXT("🚀 스프린트 활성화! (지속시간 %.1f초)"), Duration));
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
