// Copyright ExFrameWork. All Rights Reserved.

#include "ExItemEffect_Buff.h"
#include "ExItemSystemTypes.h"
#include "ExItemTags.h"
#include "ExGameplayEventSubsystem.h"

void UExItemEffect_Buff::Execute_Implementation(AActor* Instigator, const UExItemDefinition* ItemDefinition, AExItemPickupBase* ItemActor)
{
	if (!ensureAlwaysMsgf(Instigator, TEXT("[ExItemEffect_Buff] Instigator가 null입니다.")))
	{
		return;
	}

	if (!ensureAlwaysMsgf(BuffTag.IsValid(), TEXT("[ExItemEffect_Buff] BuffTag가 설정되지 않았습니다!")))
	{
		return;
	}

	UWorld* World = Instigator->GetWorld();
	if (!World)
	{
		return;
	}

	UExGameplayEventSubsystem* EventSub = World->GetSubsystem<UExGameplayEventSubsystem>();
	if (!ensureAlwaysMsgf(EventSub, TEXT("[ExItemEffect_Buff] ExGameplayEventSubsystem을 찾을 수 없습니다.")))
	{
		return;
	}

	// 버프 페이로드 구성: Magnitude를 OptionalValue에, Instigator를 주체로 설정
	FExGameplayEventPayload Payload;
	Payload.Instigator = Instigator;
	Payload.Target = Cast<AActor>(Instigator);
	Payload.OptionalValue = Magnitude;
	Payload.Duration = Duration;

	// 버프 태그 이벤트 브로드캐스트 (수신 측: MovementComp, StatComp 등)
	EventSub->BroadcastEvent(BuffTag, Payload);

	// 공통 아이템 획득 이벤트도 브로드캐스트 (UI/사운드 구독용)
	EventSub->BroadcastEventSimple(TAG_Ex_Item_PickedUp_Buff, Instigator);

	UE_LOG(LogExItemSystem, Verbose, TEXT("[ExItemEffect_Buff] %s에게 버프 적용: %s (크기: %.2f, 지속: %.1f초)"),
		*Instigator->GetName(), *BuffTag.ToString(), Magnitude, Duration);
}

FText UExItemEffect_Buff::GetEffectDescription_Implementation() const
{
	return FText::Format(
		NSLOCTEXT("ExItemEffect", "BuffDesc", "버프: {0} (크기: {1}, 지속: {2}초)"),
		FText::FromString(BuffTag.ToString()),
		FText::AsNumber(Magnitude),
		FText::AsNumber(Duration)
	);
}
