#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagContainer.h"
#include "ExGameplayEventSubsystem.h" // For FExGameplayEventPayload struct
#include "ExPawn.generated.h"

// Delegate for local events
// Delegate for local events
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FExPawnGameplayEvent, FGameplayTag, EventTag, const FExGameplayEventPayload&, Payload);

/**
 * ExFrameWork default Pawn class with GameplayTag event system integration.
 */
UCLASS()
class EXCORERUNTIME_API AExPawn : public APawn
{
	GENERATED_BODY()

public:
	AExPawn();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/**
	 * Send a gameplay event to locally bound listeners.
	 * Can optionally forward to global subsystem if needed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ex|Event")
	void SendGameplayEvent(FGameplayTag EventTag, const FExGameplayEventPayload& Payload);

	/**
	 * Delegate for subscribing to local gameplay events.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Ex|Event")
	FExPawnGameplayEvent OnGameplayEvent;
};
