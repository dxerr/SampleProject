// Copyright ExFrameWork. All Rights Reserved.

#include "ExRunnerRule_FallDeath.h"
#include "../Components/ExRunnerRuleManagerComponent.h"
#include "../GameModes/ExRunnerGameMode.h"
#include "../Tags/ExRunnerTags.h"
#include "Events/ExGameplayEventSubsystem.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuleFallDeath, Log, All);

UExRunnerRule_FallDeath::UExRunnerRule_FallDeath()
{
	bTickEnabled = false;
	TriggerTag   = TAG_Rule_FallDeath;
	DeathVolumeTag = TAG_Player_DeathVolume;
	RuleScope = EExRunnerRuleScope::Individual; // 개별 탈락 룰
}

void UExRunnerRule_FallDeath::ActivateRule()
{
	Super::ActivateRule();

	// TObjectPtr을 일반 포인터로 명시적 변환하여 FindComponentByClass 호출
	AExRunnerGameMode* GM = CachedGameMode.Get();
	if (UExRunnerRuleManagerComponent* RuleMgr = GM
		? GM->FindComponentByClass<UExRunnerRuleManagerComponent>()
		: nullptr)
	{
		CachedKillVolume = RuleMgr->SpawnKillVolume(KillVolumeZ);
		if (UBoxComponent* Box = Cast<UBoxComponent>(CachedKillVolume))
		{
			Box->OnComponentBeginOverlap.AddDynamic(this, &UExRunnerRule_FallDeath::OnKillVolumeOverlap);
			UE_LOG(LogRuleFallDeath, Log, TEXT("[FallDeath] Kill Volume 배치 완료 (Z=%.1f)"), KillVolumeZ);
		}
	}
}

void UExRunnerRule_FallDeath::DeactivateRule()
{
	if (UBoxComponent* Box = Cast<UBoxComponent>(CachedKillVolume))
	{
		Box->OnComponentBeginOverlap.RemoveDynamic(this, &UExRunnerRule_FallDeath::OnKillVolumeOverlap);
	}
	CachedKillVolume = nullptr;

	Super::DeactivateRule();
}

void UExRunnerRule_FallDeath::OnKillVolumeOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 플레이어 폰만 감지
	if (!OtherActor || !OtherActor->IsA<APawn>()) return;

	UE_LOG(LogRuleFallDeath, Log, TEXT("[FallDeath] 플레이어 낙사 감지: %s"), *OtherActor->GetName());

	// EventSubsystem으로 DeathVolume 태그 브로드캐스트 → RuleManagerComponent가 FallDeath로 처리
	if (CachedEventSubsystem)
	{
		FExGameplayEventPayload Payload;
		Payload.Instigator = OtherActor;
		CachedEventSubsystem->BroadcastEvent(DeathVolumeTag, Payload);
	}

	// 직접 룰 발동 (EventSubsystem 없을 경우 폴백 포함)
	OnRuleTriggered.Broadcast(TriggerTag, OtherActor, this);
}
