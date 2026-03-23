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
	// NOTE: Duration은 현재 FExGameplayEventPayload에 전용 필드가 없으므로
	// 수신 측에서 BuffTag별로 Duration을 해석하거나, 페이로드 확장이 필요할 수 있다.
	// 향후 FExGameplayEventPayload에 Duration 필드를 추가하는 것을 권장한다.

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
