// Copyright ExFrameWork. All Rights Reserved.

#include "ExRunnerRule_Timer.h"
#include "../GameModes/ExRunnerGameMode.h"
#include "../GameStates/ExRunnerGameState.h"
#include "../Tags/ExRunnerTags.h"
#include "Events/ExGameplayEventSubsystem.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuleTimer, Log, All);

UExRunnerRule_Timer::UExRunnerRule_Timer()
{
	bTickEnabled = true;
	TriggerTag   = TAG_Rule_TimeUp;
}

void UExRunnerRule_Timer::ActivateRule()
{
	Super::ActivateRule();
	ServerRemainingTime  = TotalTime;
	LastBroadcastSecond  = FMath::FloorToInt(TotalTime) + 1; // 첫 틱에서 반드시 전송되도록
	bWarningBroadcasted  = false;

	// 초기 시간 즉시 전송
	if (AExRunnerGameState* GS = CachedGameMode
		? CachedGameMode->GetWorld()->GetGameState<AExRunnerGameState>()
		: nullptr)
	{
		GS->SetRemainingTimeSeconds(FMath::FloorToInt(TotalTime));
		LastBroadcastSecond = FMath::FloorToInt(TotalTime);
	}

	UE_LOG(LogRuleTimer, Log, TEXT("[Rule_Timer] 활성화 — 제한시간: %.1f초, 경고: %.1f초"), TotalTime, WarningTime);
}

void UExRunnerRule_Timer::DeactivateRule()
{
	Super::DeactivateRule();
}

void UExRunnerRule_Timer::TickRule(float DeltaTime)
{
	if (ServerRemainingTime <= 0.f) return;

	ServerRemainingTime -= DeltaTime;
	ServerRemainingTime  = FMath::Max(0.f, ServerRemainingTime);

	// 정수 초 단위 변경 시에만 GameState에 전송 (초당 1회 - 대역폭 최적화)
	const int32 NewSecond = FMath::FloorToInt(ServerRemainingTime);
	if (NewSecond != LastBroadcastSecond)
	{
		LastBroadcastSecond = NewSecond;
		if (AExRunnerGameState* GS = CachedGameMode
			? CachedGameMode->GetWorld()->GetGameState<AExRunnerGameState>()
			: nullptr)
		{
			GS->SetRemainingTimeSeconds(NewSecond);
		}
	}

	// 경고 구간 진입 (최초 1회)
	if (!bWarningBroadcasted && ServerRemainingTime <= WarningTime)
	{
		bWarningBroadcasted = true;
		UE_LOG(LogRuleTimer, Log, TEXT("[Rule_Timer] 경고 구간 진입 (%.1f초 남음)"), ServerRemainingTime);
		if (CachedEventSubsystem)
		{
			CachedEventSubsystem->BroadcastEventSimple(TAG_Rule_Timer_Warning, this);
		}
	}

	// 시간 초과
	if (ServerRemainingTime <= 0.f)
	{
		UE_LOG(LogRuleTimer, Log, TEXT("[Rule_Timer] 타임오버!"));
		OnRuleTriggered.Broadcast(TriggerTag, nullptr, this);
	}
}
