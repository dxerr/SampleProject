// Copyright ExFrameWork. All Rights Reserved.

#include "ExGameplayEventSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogExGameplayEvent, Log, All);

void UExGameplayEventSubsystem::BroadcastEvent(FGameplayTag EventTag, const FExGameplayEventPayload& Payload)
{
	if (!EventTag.IsValid())
	{
		UE_LOG(LogExGameplayEvent, Warning, TEXT("BroadcastEvent: Invalid EventTag"));
		return;
	}

	UE_LOG(LogExGameplayEvent, Log, TEXT("BroadcastEvent: %s (Instigator: %s)"), 
		*EventTag.ToString(), 
		Payload.Instigator ? *Payload.Instigator->GetName() : TEXT("None"));

	// C++ 태그별 리스너에게 브로드캐스트
	if (FExGameplayEventDelegate* Delegate = EventDelegates.Find(EventTag))
	{
		Delegate->Broadcast(EventTag, Payload);
	}

	// BP용 범용 이벤트에도 브로드캐스트
	if (OnGameplayEvent.IsBound())
	{
		OnGameplayEvent.Broadcast(EventTag, Payload);
	}
}

void UExGameplayEventSubsystem::BroadcastEventSimple(FGameplayTag EventTag, UObject* Instigator)
{
	FExGameplayEventPayload Payload;
	Payload.Instigator = Instigator;
	BroadcastEvent(EventTag, Payload);
}

FExGameplayEventDelegate& UExGameplayEventSubsystem::GetEventDelegate(FGameplayTag EventTag)
{
	UE_LOG(LogExGameplayEvent, Log, TEXT("GetEventDelegate: Registering listener for tag: %s"), *EventTag.ToString());
	return EventDelegates.FindOrAdd(EventTag);
}

bool UExGameplayEventSubsystem::HasListeners(FGameplayTag EventTag) const
{
	if (const FExGameplayEventDelegate* Delegate = EventDelegates.Find(EventTag))
	{
		return Delegate->IsBound();
	}
	return false;
}
