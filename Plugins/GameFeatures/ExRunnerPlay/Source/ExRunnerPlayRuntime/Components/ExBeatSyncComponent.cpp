// Copyright ExFrameWork. All Rights Reserved.

#include "ExBeatSyncComponent.h"
#include "ExObstacleManager.h"
#include "ExGameplayEventSubsystem.h"
#include "Tags/ExMusicTags.h"
#include "Math/UnrealMathUtility.h"
#include "Data/ExRunnerConfig.h"
#include "Subsystems/ExDataCenterSubsystem.h"
#include "Engine/GameInstance.h"

UExBeatSyncComponent::UExBeatSyncComponent()
	: BoundObstacleManager(nullptr)
	, LastBeatSpawnTime(-999.0f)
	, MinBeatSpawnInterval(0.2f)
{
	PrimaryComponentTick.bCanEverTick = false;
	bRuntimeBeatSyncEnabled = true;
}

void UExBeatSyncComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UExDataCenterSubsystem* DC = GI->GetSubsystem<UExDataCenterSubsystem>())
			{
				CachedConfig = DC->GetConfig<UExRunnerConfig>();
			}
		}

		if (CachedConfig)
		{
			bRuntimeBeatSyncEnabled = CachedConfig->BeatSync.bBeatSyncEnabled;
		}

		// 전역 이벤트 서브시스템에서 비트 이벤트 구독
		if (UExGameplayEventSubsystem* EventSubsystem = World->GetSubsystem<UExGameplayEventSubsystem>())
		{
			// TAG_Music_Beat은 매 비트마다 발생
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

void UExBeatSyncComponent::BindToObstacleManager(UExObstacleManager* InManager)
{
	BoundObstacleManager = InManager;
	if (BoundObstacleManager)
	{
		// 초기 상태 동기화
		BoundObstacleManager->bSuppressDefaultChunkSpawn = bRuntimeBeatSyncEnabled;
	}
}

void UExBeatSyncComponent::SetBeatSyncEnabled(bool bEnabled)
{
	bRuntimeBeatSyncEnabled = bEnabled;
	if (BoundObstacleManager)
	{
		BoundObstacleManager->bSuppressDefaultChunkSpawn = bRuntimeBeatSyncEnabled;
	}
}

void UExBeatSyncComponent::OnMusicBeat(FGameplayTag EventTag, const FExGameplayEventPayload& Payload)
{
	if (!bRuntimeBeatSyncEnabled || !BoundObstacleManager)
		return;

	float CurrentTime = GetWorld()->GetTimeSeconds();
	
	// 최소 간격 방어코드
	if (CurrentTime - LastBeatSpawnTime < MinBeatSpawnInterval)
		return;

	float FinalProbability = CachedConfig ? CachedConfig->BeatSync.SpawnProbabilityPerBeat : 0.5f;

	// Payload에 비트 인덱스나 강박 정보가 있다면 참조 (여기서는 단순 태그 보너스 예시로 Music_Beat_Strong일 경우 보너스 추가)
	// Payload 대신 향후 EventTag가 Music_Beat_Strong도 같이 받을 수 있으나
	// 일단 짝수 비트를 강박으로 임시 간주 (원격 이벤트에서 구별해서 줄 경우 수정)
	// 하지만 현재 Music_Beat 태그만 핸들링하므로 기본 확률 적용.

	if (FMath::FRand() <= FinalProbability)
	{
		BoundObstacleManager->RequestBeatSpawn();
		LastBeatSpawnTime = CurrentTime;
	}
}
