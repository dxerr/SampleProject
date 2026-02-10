
#include "ExPawn.h"

AExPawn::AExPawn()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

void AExPawn::BeginPlay()
{
	Super::BeginPlay();
	
}

void AExPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AExPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

void AExPawn::SendGameplayEvent(FGameplayTag EventTag, const FExGameplayEventPayload& Payload)
{
	// Broadcast locally to children or components
	if (OnGameplayEvent.IsBound())
	{
		OnGameplayEvent.Broadcast(EventTag, Payload);
	}
}
