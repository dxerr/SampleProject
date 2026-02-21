// Copyright ExFrameWork. All Rights Reserved.

#include "ExDebugStateSubsystem.h"
#include "ExGameplayEventSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogExDebugState, Log, All);

void UExDebugStateSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogExDebugState, Log, TEXT("ExDebugStateSubsystem 초기화 — 전역 디버그 상태 관리 시작"));
}

void UExDebugStateSubsystem::Deinitialize()
{
	UE_LOG(LogExDebugState, Log, TEXT("ExDebugStateSubsystem 해제 — 상태 맵 클리어"));
	CheatStates.Empty();
	Super::Deinitialize();
}

bool UExDebugStateSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Development/Debug 빌드에서만 활성화, Shipping 빌드에서는 비활성화
#if UE_BUILD_SHIPPING
	return false;
#else
	return true;
#endif
}

// ========== Toggle 관리 ==========

void UExDebugStateSubsystem::SetCheatEnabled(FGameplayTag CheatTag, bool bEnabled)
{
	if (!CheatTag.IsValid())
	{
		UE_LOG(LogExDebugState, Warning, TEXT("SetCheatEnabled: 유효하지 않은 태그"));
		return;
	}

	FExDebugCheatState& State = CheatStates.FindOrAdd(CheatTag);
	if (State.bEnabled != bEnabled)
	{
		State.bEnabled = bEnabled;
		UE_LOG(LogExDebugState, Log, TEXT("치트 상태 변경: [%s] = %s"), 
			*CheatTag.ToString(), bEnabled ? TEXT("ON") : TEXT("OFF"));
		BroadcastStateChange(CheatTag);
	}
}

bool UExDebugStateSubsystem::IsCheatEnabled(FGameplayTag CheatTag) const
{
	if (const FExDebugCheatState* State = CheatStates.Find(CheatTag))
	{
		return State->bEnabled;
	}
	return false;
}

bool UExDebugStateSubsystem::ToggleCheat(FGameplayTag CheatTag)
{
	const bool bNewState = !IsCheatEnabled(CheatTag);
	SetCheatEnabled(CheatTag, bNewState);
	return bNewState;
}

// ========== Slider 수치 관리 ==========

void UExDebugStateSubsystem::SetCheatValue(FGameplayTag CheatTag, float Value)
{
	if (!CheatTag.IsValid())
	{
		UE_LOG(LogExDebugState, Warning, TEXT("SetCheatValue: 유효하지 않은 태그"));
		return;
	}

	FExDebugCheatState& State = CheatStates.FindOrAdd(CheatTag);
	State.Value = Value;
	UE_LOG(LogExDebugState, Log, TEXT("치트 수치 변경: [%s] = %.2f"), 
		*CheatTag.ToString(), Value);
	BroadcastStateChange(CheatTag);
}

float UExDebugStateSubsystem::GetCheatValue(FGameplayTag CheatTag) const
{
	if (const FExDebugCheatState* State = CheatStates.Find(CheatTag))
	{
		return State->Value;
	}
	return 0.f;
}

// ========== 전체 상태 ==========

void UExDebugStateSubsystem::ResetAllStates()
{
	CheatStates.Empty();
	UE_LOG(LogExDebugState, Log, TEXT("모든 디버그 상태가 초기화되었습니다."));
}

void UExDebugStateSubsystem::PrintAllStates() const
{
	if (CheatStates.Num() == 0)
	{
		UE_LOG(LogExDebugState, Log, TEXT("===== 활성화된 치트 없음 ====="));
		return;
	}

	UE_LOG(LogExDebugState, Log, TEXT("===== ExDebug 상태 목록 ====="));
	for (const auto& Pair : CheatStates)
	{
		const FExDebugCheatState& State = Pair.Value;
		UE_LOG(LogExDebugState, Log, TEXT("  [%s] Enabled=%s  Value=%.2f  Index=%d"),
			*Pair.Key.ToString(),
			State.bEnabled ? TEXT("ON") : TEXT("OFF"),
			State.Value,
			State.SelectedIndex);
	}
	UE_LOG(LogExDebugState, Log, TEXT("============================="));
}

// ========== 이벤트 브로드캐스트 ==========

void UExDebugStateSubsystem::BroadcastStateChange(FGameplayTag CheatTag)
{
	// GameInstance에서 World를 가져와 ExGameplayEventSubsystem에 접근
	const UGameInstance* GI = GetGameInstance();
	if (!GI) return;

	const UWorld* World = GI->GetWorld();
	if (!World) return;

	UExGameplayEventSubsystem* EventSub = World->GetSubsystem<UExGameplayEventSubsystem>();
	if (!EventSub) return;

	// 기존 ExGameplayEventSubsystem의 Pub/Sub 프로토콜 활용
	FExGameplayEventPayload Payload;
	Payload.Instigator = const_cast<UGameInstance*>(GI);

	const FExDebugCheatState* State = CheatStates.Find(CheatTag);
	if (State)
	{
		Payload.OptionalValue = State->bEnabled ? 1.f : 0.f;
	}

	EventSub->BroadcastEvent(CheatTag, Payload);
}
