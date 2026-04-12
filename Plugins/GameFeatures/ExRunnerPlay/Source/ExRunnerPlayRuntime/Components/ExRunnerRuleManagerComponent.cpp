// Copyright ExFrameWork. All Rights Reserved.

#include "ExRunnerRuleManagerComponent.h"
#include "../Rules/ExRunnerRuleBase.h"
#include "../Data/ExRunnerRuleConfig.h"
#include "../GameModes/ExRunnerGameMode.h"
#include "../GameStates/ExRunnerGameState.h"
#include "../Tags/ExRunnerTags.h"
#include "Events/ExGameplayEventSubsystem.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogExRuleManager, Log, All);

UExRunnerRuleManagerComponent::UExRunnerRuleManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // ActivateAllRules() 시 활성화
}

void UExRunnerRuleManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AExRunnerGameMode* GM = Cast<AExRunnerGameMode>(GetOwner()))
	{
		CachedGameState = GM->GetWorld()->GetGameState<AExRunnerGameState>();
		CachedEventSubsystem = GM->GetWorld()->GetSubsystem<UExGameplayEventSubsystem>();
	}
}

void UExRunnerRuleManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	for (UExRunnerRuleBase* Rule : ActiveRules)
	{
		if (Rule && Rule->bTickEnabled)
		{
			Rule->TickRule(DeltaTime);
		}
	}
}

void UExRunnerRuleManagerComponent::ActivateAllRules()
{
	if (!RuleConfig)
	{
		UE_LOG(LogExRuleManager, Warning, TEXT("[RuleManager] RuleConfig가 설정되지 않았습니다. 룰 없이 게임 진행됩니다."));
		return;
	}

	AExRunnerGameMode* GM = Cast<AExRunnerGameMode>(GetOwner());
	if (!GM) return;

	bGameOverHandled = false;
	ActiveRules.Reset();

	for (UExRunnerRuleBase* Rule : RuleConfig->Rules)
	{
		if (!Rule) continue;

		Rule->InitializeRule(GM);
		Rule->OnRuleTriggered.AddUObject(this, &UExRunnerRuleManagerComponent::OnRuleTriggered);
		Rule->ActivateRule();
		ActiveRules.Add(Rule);

		UE_LOG(LogExRuleManager, Log, TEXT("[RuleManager] 룰 활성화: %s"), *Rule->GetClass()->GetName());
	}

	// Tick 활성화 (TickEnabled 룰이 하나라도 있을 때)
	bool bAnyTickEnabled = ActiveRules.ContainsByPredicate([](const UExRunnerRuleBase* R) { return R && R->bTickEnabled; });
	PrimaryComponentTick.SetTickFunctionEnable(bAnyTickEnabled);

	UE_LOG(LogExRuleManager, Log, TEXT("[RuleManager] 총 %d개 룰 활성화 완료"), ActiveRules.Num());
}

void UExRunnerRuleManagerComponent::DeactivateAllRules()
{
	PrimaryComponentTick.SetTickFunctionEnable(false);

	for (UExRunnerRuleBase* Rule : ActiveRules)
	{
		if (Rule)
		{
			Rule->OnRuleTriggered.RemoveAll(this);
			Rule->DeactivateRule();
		}
	}

	ActiveRules.Reset();
	bGameOverHandled = false;

	// Kill Volume 정리
	if (SpawnedKillVolumeComp)
	{
		SpawnedKillVolumeComp->DestroyComponent();
		SpawnedKillVolumeComp = nullptr;
	}

	UE_LOG(LogExRuleManager, Log, TEXT("[RuleManager] 모든 룰 비활성화 완료"));
}

UShapeComponent* UExRunnerRuleManagerComponent::SpawnKillVolume(float KillVolumeZ)
{
	// 중복 생성 방지
	if (SpawnedKillVolumeComp)
	{
		UE_LOG(LogExRuleManager, Log, TEXT("[RuleManager] Kill Volume이 이미 존재합니다. Skip."));
		return SpawnedKillVolumeComp;
	}

	AActor* Owner = GetOwner();
	if (!Owner) return nullptr;

	UBoxComponent* KillVolume = NewObject<UBoxComponent>(Owner, TEXT("KillVolume"));
	KillVolume->SetBoxExtent(FVector(20000.f, 20000.f, 100.f));
	KillVolume->SetWorldLocation(FVector(0.f, 0.f, KillVolumeZ));
	KillVolume->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	KillVolume->SetGenerateOverlapEvents(true);
	KillVolume->RegisterComponent();

	SpawnedKillVolumeComp = KillVolume;
	UE_LOG(LogExRuleManager, Log, TEXT("[RuleManager] Kill Volume 생성 완료 (Z=%.1f)"), KillVolumeZ);

	return KillVolume;
}

void UExRunnerRuleManagerComponent::OnRuleTriggered(FGameplayTag ResultTag)
{
	if (!GetOwner()->HasAuthority()) return;
	if (bGameOverHandled) return; // 복수 룰 동시 발동 방지

	bGameOverHandled = true;

	UE_LOG(LogExRuleManager, Log, TEXT("[RuleManager] 룰 발동: %s"), *ResultTag.ToString());

	// [1] GameState에 게임오버 원인 설정 → 클라이언트 리플리케이션
	if (CachedGameState)
	{
		CachedGameState->SetGameOverReason(TagToReason(ResultTag));
	}

	// [2] EventSubsystem으로 브로드캐스트 → GameMode가 EndMatch 처리
	if (CachedEventSubsystem)
	{
		FExGameplayEventPayload Payload;
		Payload.Instigator = this;
		CachedEventSubsystem->BroadcastEvent(ResultTag, Payload);
	}
}

EExRunnerGameOverReason UExRunnerRuleManagerComponent::TagToReason(const FGameplayTag& ResultTag) const
{
	if (ResultTag == TAG_Rule_FallDeath)    return EExRunnerGameOverReason::FallDeath;
	if (ResultTag == TAG_Rule_TimeUp)       return EExRunnerGameOverReason::TimeUp;
	if (ResultTag == TAG_Rule_GoalReached)  return EExRunnerGameOverReason::GoalReached;
	return EExRunnerGameOverReason::None;
}
