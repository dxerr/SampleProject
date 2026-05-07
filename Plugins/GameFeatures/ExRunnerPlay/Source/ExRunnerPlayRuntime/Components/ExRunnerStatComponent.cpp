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
#include "Net/UnrealNetwork.h"
#include "../GameStates/ExRunnerGameState.h"

DEFINE_LOG_CATEGORY_STATIC(LogExRunnerStatComp, Log, All);

UExRunnerStatComponent::UExRunnerStatComponent()
{
	// 순수 데이터 모델이므로 Tick 갱신 오버헤드를 원천 차단합니다.
	// 대신 타이머(StatPollInterval)를 사용하여 주기적으로 스탯을 수집합니다.
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);
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

void UExRunnerStatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UExRunnerStatComponent, bIsSprintBuffActive);
	DOREPLIFETIME(UExRunnerStatComponent, SprintRemainingTime);
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

	// ─── Distance ──────────────────────────────────────────────────────────────
	if (AExRunnerGameState* GS = GetWorld()->GetGameState<AExRunnerGameState>())
	{
		// 서버와 동기화된 RealPlayerPathDistance 또는 로컬 CurrentPathDistance 사용
		SetCurrentDistance(GS->CurrentPathDistance);
	}
	
	UpdateSprintTimer();
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

void UExRunnerStatComponent::UpdateSprintTimer()
{
	// 오직 서버 권한에서만 타이머를 감소시키며 클라이언트는 OnRep를 통해 UI만 갱신합니다.
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (SprintRemainingTime > 0.0f)
		{
			SprintRemainingTime -= StatPollInterval;
			
			// 매 틱마다 잔여시간을 서버 로컬 호스트 UI에 갱신
			OnSprintTimeChanged.Broadcast(SprintRemainingTime);

			if (SprintRemainingTime <= 0.0f)
			{
				SprintRemainingTime = 0.0f;
				
				// 스프린트 종료
				if (bIsSprintBuffActive)
				{
					bIsSprintBuffActive = false;
					OnRep_IsSprintBuffActive(); // 서버 환경/스탠드얼론에서는 OnRep가 자동 호출되지 않으므로 수동 호출
				}
				
				UE_LOG(LogExRunnerStatComp, Log, TEXT("[ExRunnerStatComponent] 스프린트 종료 (타이머 만료)"));
				
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("스프린트 버프 해제됨!"));
				}
			}
			
			if (SprintRemainingTime > 0.0f && GEngine)
			{
				// 매 틱 로그 도배 방지용 고정 키(2)를 사용하여 텍스트 라인 재사용
				GEngine->AddOnScreenDebugMessage(2, 0.5f, FColor::Yellow, FString::Printf(TEXT("남은 스프린트 시간: %.1f초"), SprintRemainingTime));
			}
		}
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

void UExRunnerStatComponent::OnSpeedUpBuff(FGameplayTag Tag, const FExGameplayEventPayload& Payload)
{
	// 이 이벤트를 유발한 대상(아이템을 먹은 캐릭터)이 나 자신인지 확인
	if (Payload.Instigator != BoundPawn.Get() && Payload.Target != BoundPawn.Get())
	{
		return;
	}

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

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		SprintRemainingTime = Duration; // 남은 시간 갱신/초기화 (서버 전용)

		if (!bIsSprintBuffActive)
		{
			bIsSprintBuffActive = true;
			OnRep_IsSprintBuffActive(); // 서버 환경/스탠드얼론에서는 OnRep가 자동 호출되지 않으므로 수동 호출
		}
		
		OnSprintTimeChanged.Broadcast(SprintRemainingTime); // 서버 로컬 호스트용 UI 즉시 갱신
	}

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

void UExRunnerStatComponent::OnRep_IsSprintBuffActive()
{
	if (CachedInputComponent.IsValid())
	{
		CachedInputComponent->RequestSprintAction(bIsSprintBuffActive);
	}
	UE_LOG(LogExRunnerStatComp, Log, TEXT("[ExRunnerStatComponent] Sprint Buff State changed to: %s"), bIsSprintBuffActive ? TEXT("True") : TEXT("False"));
}

void UExRunnerStatComponent::OnRep_SprintRemainingTime()
{
	// 서버에서 갱신된 타이머 값을 받아 로컬 UI(프로그레스 바 등)를 업데이트합니다.
	OnSprintTimeChanged.Broadcast(SprintRemainingTime);
}
