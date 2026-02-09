// Copyright ExFrameWork. All Rights Reserved.

#include "ExGameplayEventLibrary.h"
#include "ExGameplayEventSubsystem.h"


void UExGameplayEventLibrary::BroadcastGameplayEvent(UObject* WorldContextObject, FGameplayTag EventTag, UObject* Instigator)
{
	if (!WorldContextObject) return;
	
	UWorld* World = WorldContextObject->GetWorld();
	if (!World) return;
	
	if (UExGameplayEventSubsystem* Subsystem = World->GetSubsystem<UExGameplayEventSubsystem>())
	{
		Subsystem->BroadcastEventSimple(EventTag, Instigator);
	}
}

UExGameplayEventSubsystem* UExGameplayEventLibrary::GetExGameplayEventSubsystem(UObject* WorldContextObject)
{
	if (!WorldContextObject) return nullptr;
	
	UWorld* World = WorldContextObject->GetWorld();
	if (!World) return nullptr;
	
	return World->GetSubsystem<UExGameplayEventSubsystem>();
}
