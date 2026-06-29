// Copyright ExFrameWork. All Rights Reserved.

#include "ExBeatSyncComponent.h"
#include "ExGameplayEventSubsystem.h"
#include "Tags/ExMusicTags.h"
#include "Math/UnrealMathUtility.h"

UExBeatSyncComponent::UExBeatSyncComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UExBeatSyncComponent::InitSettings(const FExBeatSyncSettings& Settings)
{
	CurrentSettings = Settings;
	SetBeatSyncEnabled(Settings.bBeatSyncEnabled);
}

void UExBeatSyncComponent::BeginPlay()
{
	Super::BeginPlay();

	// InitSettings()가 BeginPlay 이전에 호출되지 않았다면 기본값을 사용
	bRuntimeBeatSyncEnabled = CurrentSettings.bBeatSyncEnabled;

	if (UWorld* World = GetWorld())
	{
		if (UExGameplayEventSubsystem* EventSubsystem = World->GetSubsystem<UExGameplayEventSubsystem>())
		{
			EventSubsystem->GetEventDelegate(ExMusicTags::Music_Beat).AddDynamic(this, &UExBeatSyncComponent::OnMusicBeat);
		}
	}
}

void UExBeatSyncComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UExGameplayEventSubsystem* EventSubsystem = World->GetSubsystem<UExGameplayEventSubsystem>())
		{
			EventSubsystem->GetEventDelegate(ExMusicTags::Music_Beat).RemoveDynamic(this, &UExBeatSyncComponent::OnMusicBeat);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UExBeatSyncComponent::SetBeatSyncEnabled(bool bEnabled)
{
	bRuntimeBeatSyncEnabled = bEnabled;
	OnBeatSyncStateChanged.Broadcast(bEnabled);
}

void UExBeatSyncComponent::OnMusicBeat(FGameplayTag EventTag, const FExGameplayEventPayload& Payload)
{
	const int32 CurrentBeatIndex = BeatIndex++;

	if (!bRuntimeBeatSyncEnabled)
		return;

	const float CurrentTime = GetWorld()->GetTimeSeconds();

	if (CurrentTime - LastBeatFireTime < MinBeatFireInterval)
		return;

	if (FMath::FRand() <= CurrentSettings.SpawnProbabilityPerBeat)
	{
		OnBeatTick.Broadcast(CurrentBeatIndex, CurrentTime);
		LastBeatFireTime = CurrentTime;
	}
}
