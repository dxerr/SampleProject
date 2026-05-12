// Copyright ExFrameWork. All Rights Reserved.

#include "ExRunnerBuffComponent.h"
#include "ExRunnerInputComponent.h"
#include "ExRunnerMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Pawn.h"
#include "Events/ExGameplayEventSubsystem.h"
#include "ExItemTags.h"
#include "Util/Actor/ExActorUtil.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY_STATIC(LogExRunnerBuff, Log, All);

UExRunnerBuffComponent::UExRunnerBuffComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UExRunnerBuffComponent::BeginPlay()
{
	Super::BeginPlay();

	// Owner → AttachParent 순으로 Pawn 탐색
	BoundPawn = UExActorUtil::FindOwnerPawn(this);
	if (!BoundPawn.IsValid())
	{
		UE_LOG(LogExRunnerBuff, Warning, TEXT("[ExRunnerBuffComponent] Pawn을 찾을 수 없습니다."));
		return;
	}

	// 컴포넌트 캐싱
	CachedInputComp    = BoundPawn->FindComponentByClass<UExRunnerInputComponent>();
	CachedMovementComp = BoundPawn->FindComponentByClass<UExRunnerMovementComponent>();

	if (!CachedInputComp.IsValid())
	{
		UE_LOG(LogExRunnerBuff, Warning, TEXT("[ExRunnerBuffComponent] ExRunnerInputComponent를 찾을 수 없습니다."));
	}
	if (!CachedMovementComp.IsValid())
	{
		UE_LOG(LogExRunnerBuff, Warning, TEXT("[ExRunnerBuffComponent] ExRunnerMovementComponent를 찾을 수 없습니다."));
	}

	// ExGameplayEventSubsystem 이벤트 구독
	if (UWorld* World = GetWorld())
	{
		if (UExGameplayEventSubsystem* EventSub = World->GetSubsystem<UExGameplayEventSubsystem>())
		{
			EventSub->GetEventDelegate(TAG_Ex_Buff_SpeedUp).AddDynamic(this,   &UExRunnerBuffComponent::OnSpeedUpBuffEvent);
			EventSub->GetEventDelegate(TAG_Ex_Buff_SpeedDown).AddDynamic(this, &UExRunnerBuffComponent::OnSpeedDownBuffEvent);

			// Match_Playing 전환 시 Sprint 활성화 이벤트 구독
			// (이전: BeginPlay에서 직접 RequestSprintAction 호출 시도
			//  IsMatchActive()=false로 인해 차단되던 버그 수정)
			FGameplayTag MatchPlayingTag = FGameplayTag::RequestGameplayTag(FName("Match.Playing"));
			EventSub->GetEventDelegate(MatchPlayingTag).AddDynamic(this, &UExRunnerBuffComponent::OnMatchPlayingStarted);
		}
	}

	// 타이머 폴링 시작 (서버 권한에서만 의미 있음)
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		GetWorld()->GetTimerManager().SetTimer(
			PollTimerHandle,
			this,
			&UExRunnerBuffComponent::UpdateBuffTimers,
			PollInterval,
			true
		);
	}
}

void UExRunnerBuffComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UExRunnerBuffComponent, ActiveBuffs);
}

// ─────────────────────────────────────────────────────────────
// ActivateBuff: 버프 활성화 (서버 권한)
// ─────────────────────────────────────────────────────────────
void UExRunnerBuffComponent::ActivateBuff(FExBuffDefinition Def)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (Def.Duration <= 0.0f) return;

	// 1. RemoveList에 있는 버프 먼저 종료
	for (EExBuffType RemoveType : Def.RemoveList)
	{
		TerminateBuff(RemoveType);
	}

	// 2. 동일 타입 버프가 이미 있으면 Duration 연장
	for (FExActiveBuffState& Existing : ActiveBuffs)
	{
		if (Existing.BuffType == Def.BuffType)
		{
			Existing.RemainingTime += Def.Duration;
			Existing.Weight = Def.Weight;

			UE_LOG(LogExRunnerBuff, Log, TEXT("[ExRunnerBuff] 버프 연장: %s, 남은시간: %.1f초"),
				*UEnum::GetValueAsString(Def.BuffType), Existing.RemainingTime);

			OnBuffActivated.Broadcast(Def.BuffType, Existing.RemainingTime);
			return;
		}
	}

	// 3. 신규 버프 등록
	FExActiveBuffState NewBuff;
	NewBuff.BuffType      = Def.BuffType;
	NewBuff.RemainingTime = Def.Duration;
	NewBuff.Weight        = Def.Weight;
	ActiveBuffs.Add(NewBuff);

	// 4. 버프 효과 적용
	ApplyBuffEffect(NewBuff);

	UE_LOG(LogExRunnerBuff, Log, TEXT("[ExRunnerBuff] 버프 활성화: %s, 지속: %.1f초, Weight: %.2f"),
		*UEnum::GetValueAsString(Def.BuffType), Def.Duration, Def.Weight);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange,
			FString::Printf(TEXT("[Buff] %s 활성화 (%.1f초)"), *UEnum::GetValueAsString(Def.BuffType), Def.Duration));
	}

	OnBuffActivated.Broadcast(Def.BuffType, Def.Duration);
}

// ─────────────────────────────────────────────────────────────
// TerminateBuff: 특정 버프 강제 종료
// ─────────────────────────────────────────────────────────────
void UExRunnerBuffComponent::TerminateBuff(EExBuffType BuffType)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	int32 RemovedIndex = ActiveBuffs.IndexOfByPredicate(
		[BuffType](const FExActiveBuffState& S){ return S.BuffType == BuffType; });

	if (RemovedIndex == INDEX_NONE) return;

	ActiveBuffs.RemoveAt(RemovedIndex);

	// 버프 효과 복구
	RevertBuffEffect(BuffType);

	UE_LOG(LogExRunnerBuff, Log, TEXT("[ExRunnerBuff] 버프 종료: %s"), *UEnum::GetValueAsString(BuffType));

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan,
			FString::Printf(TEXT("[Buff] %s 해제"), *UEnum::GetValueAsString(BuffType)));
	}

	OnBuffDeactivated.Broadcast(BuffType);
}

// ─────────────────────────────────────────────────────────────
// ClearAllBuffs: 전체 버프 해제 (낙사 등 이벤트)
// ─────────────────────────────────────────────────────────────
void UExRunnerBuffComponent::ClearAllBuffs()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	// 복사 후 순회 (TerminateBuff가 ActiveBuffs를 수정하므로)
	TArray<EExBuffType> ToRemove;
	for (const FExActiveBuffState& Buff : ActiveBuffs)
	{
		ToRemove.Add(Buff.BuffType);
	}
	for (EExBuffType Type : ToRemove)
	{
		TerminateBuff(Type);
	}

	UE_LOG(LogExRunnerBuff, Log, TEXT("[ExRunnerBuff] 모든 버프 해제 완료"));
}

// ─────────────────────────────────────────────────────────────
// Query
// ─────────────────────────────────────────────────────────────
bool UExRunnerBuffComponent::IsBuffActive(EExBuffType BuffType) const
{
	return ActiveBuffs.ContainsByPredicate(
		[BuffType](const FExActiveBuffState& S){ return S.BuffType == BuffType; });
}

float UExRunnerBuffComponent::GetBuffRemainingTime(EExBuffType BuffType) const
{
	const FExActiveBuffState* Found = ActiveBuffs.FindByPredicate(
		[BuffType](const FExActiveBuffState& S){ return S.BuffType == BuffType; });
	return Found ? Found->RemainingTime : 0.0f;
}

// ─────────────────────────────────────────────────────────────
// UpdateBuffTimers: 폴링 타이머 (서버 권한)
// ─────────────────────────────────────────────────────────────
void UExRunnerBuffComponent::UpdateBuffTimers()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	TArray<EExBuffType> Expired;
	for (FExActiveBuffState& Buff : ActiveBuffs)
	{
		Buff.RemainingTime -= PollInterval;
		if (Buff.RemainingTime <= 0.0f)
		{
			Expired.Add(Buff.BuffType);
		}
		else
		{
			// 폴링 주기마다 잔여 시간 업데이트 전달 — UI ProgressBar 카운트다운 용
			OnBuffTimeUpdated.Broadcast(Buff.BuffType, Buff.RemainingTime);
		}
	}
	for (EExBuffType Type : Expired)
	{
		TerminateBuff(Type);
	}
}

// ─────────────────────────────────────────────────────────────
// ApplyBuffEffect: 버프 효과 적용
// ─────────────────────────────────────────────────────────────
void UExRunnerBuffComponent::ApplyBuffEffect(const FExActiveBuffState& Buff)
{
	switch (Buff.BuffType)
	{
	case EExBuffType::SpeedUp:
		// BaseMaxSpeed × Weight 로 이동 속도 증가
		if (CachedMovementComp.IsValid())
		{
			CachedMovementComp->ApplySpeedMultiplier(Buff.Weight);
		}
		break;

	case EExBuffType::SpeedDown:
		// Sprint 해제 → 걷기 상태로 전환
		if (CachedMovementComp.IsValid())
		{
			CachedMovementComp->SetWantsToSprint(false);
		}
		break;

	default:
		break;
	}
}

// ─────────────────────────────────────────────────────────────
// RevertBuffEffect: 버프 효과 복구
// ─────────────────────────────────────────────────────────────
void UExRunnerBuffComponent::RevertBuffEffect(EExBuffType BuffType)
{
	switch (BuffType)
	{
	case EExBuffType::SpeedUp:
		// 속도 배율 1.0 (원상 복구)
		if (CachedMovementComp.IsValid())
		{
			CachedMovementComp->ApplySpeedMultiplier(1.0f);
		}
		break;

	case EExBuffType::SpeedDown:
		// Sprint 재활성화
		if (CachedMovementComp.IsValid())
		{
			CachedMovementComp->SetWantsToSprint(true);
		}
		break;

	default:
		break;
	}
}

// ─────────────────────────────────────────────────────────────
// OnRep_ActiveBuffs: 클라이언트 UI 동기화
// ─────────────────────────────────────────────────────────────
void UExRunnerBuffComponent::OnRep_ActiveBuffs()
{
	// 클라이언트에서는 ActiveBuffs 배열 변경 시 UI 델리게이트만 발행
	// (실제 효과는 서버에서 적용되고 복제된 Pawn 상태로 반영됨)
	for (const FExActiveBuffState& Buff : ActiveBuffs)
	{
		OnBuffActivated.Broadcast(Buff.BuffType, Buff.RemainingTime);
	}
}

// ─────────────────────────────────────────────────────────────
// 이벤트 구독 콜백
// ─────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────
// TagToBuffType: 데이터 에셋 RemoveList(태그) → EExBuffType 변환
// ─────────────────────────────────────────────────────────────
TOptional<EExBuffType> UExRunnerBuffComponent::TagToBuffType(const FGameplayTag& Tag)
{
	// GameplayTag 스트링 매핑으로 EExBuffType 반환
	// 새 버프 타입 추가 시 여기에도 항목을 추가합니다.
	if (Tag == FGameplayTag::RequestGameplayTag(FName("Ex.Buff.SpeedUp")))
	{
		return EExBuffType::SpeedUp;
	}
	if (Tag == FGameplayTag::RequestGameplayTag(FName("Ex.Buff.SpeedDown")))
	{
		return EExBuffType::SpeedDown;
	}
	return TOptional<EExBuffType>{}; // 지원하지 않는 태그 — nullopt
}

// ─────────────────────────────────────────────────────────────
// 이벤트 구독 콜백 — 데이터 에셋 RemoveList 활용
// ─────────────────────────────────────────────────────────────
void UExRunnerBuffComponent::OnSpeedUpBuffEvent(FGameplayTag Tag, const FExGameplayEventPayload& Payload)
{
	if (Payload.Instigator != BoundPawn.Get() && Payload.Target != BoundPawn.Get()) return;

	FExBuffDefinition Def;
	Def.BuffType  = EExBuffType::SpeedUp;
	Def.Duration  = Payload.Duration > 0.0f ? Payload.Duration : 5.0f;
	Def.Weight    = Payload.OptionalValue > 0.0f ? Payload.OptionalValue : 1.3f;

	// 데이터 에셋에서 설정한 RemoveList(FGameplayTag 배열) → EExBuffType로 변환
	for (const FGameplayTag& RemoveTag : Payload.RemoveList)
	{
		if (TOptional<EExBuffType> MappedType = TagToBuffType(RemoveTag))
		{
			Def.RemoveList.Add(MappedType.GetValue());
		}
	}

	ActivateBuff(Def);
}

void UExRunnerBuffComponent::OnSpeedDownBuffEvent(FGameplayTag Tag, const FExGameplayEventPayload& Payload)
{
	if (Payload.Instigator != BoundPawn.Get() && Payload.Target != BoundPawn.Get()) return;

	FExBuffDefinition Def;
	Def.BuffType  = EExBuffType::SpeedDown;
	Def.Duration  = Payload.Duration > 0.0f ? Payload.Duration : 3.0f;
	Def.Weight    = 1.0f; // SpeedDown은 Weight 미사용

	// 데이터 에셋에서 설정한 RemoveList 변환
	for (const FGameplayTag& RemoveTag : Payload.RemoveList)
	{
		if (TOptional<EExBuffType> MappedType = TagToBuffType(RemoveTag))
		{
			Def.RemoveList.Add(MappedType.GetValue());
		}
	}

	ActivateBuff(Def);
}

void UExRunnerBuffComponent::OnClearAllBuffsEvent(FGameplayTag Tag, const FExGameplayEventPayload& Payload)
{
	if (Payload.Instigator != BoundPawn.Get() && Payload.Target != BoundPawn.Get()) return;
	ClearAllBuffs();
}

void UExRunnerBuffComponent::OnMatchPlayingStarted(FGameplayTag Tag, const FExGameplayEventPayload& Payload)
{
	// Match_Playing 전환 시점에 기본 Sprint 활성화
	// 이 시점에는 IsMatchActive() = true이므로 RequestSprintAction이 정상 통과됨
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	UE_LOG(LogExRunnerBuff, Log, TEXT("[ExRunnerBuff] Match_Playing 시작 감지 → 기본 Sprint 활성화"));

	if (CachedMovementComp.IsValid())
	{
		CachedMovementComp->SetWantsToSprint(true);
	}
	else
	{
		UE_LOG(LogExRunnerBuff, Warning, TEXT("[ExRunnerBuff] CachedMovementComp가 유효하지 않아 Sprint 적용 실패. Pawn을 확인하세요."));
	}
}
