#include "ExRunnerInputComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "InputAction.h"

void UExRunnerInputComponent::InitializeInputBindings(UEnhancedInputComponent* EnhancedInputComponent)
{
	Super::InitializeInputBindings(EnhancedInputComponent);

	if (EnhancedInputComponent)
	{
		// 점프 바인딩 (Triggered와 Started 모두 수신)
		if (JumpAction)
		{
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &UExRunnerInputComponent::NativeOnJumpAction);
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &UExRunnerInputComponent::NativeOnJumpAction);
		}

		// 슬라이드 바인딩
		if (SlideAction)
		{
			EnhancedInputComponent->BindAction(SlideAction, ETriggerEvent::Triggered, this, &UExRunnerInputComponent::NativeOnSlideAction);
			EnhancedInputComponent->BindAction(SlideAction, ETriggerEvent::Started, this, &UExRunnerInputComponent::NativeOnSlideAction);
		}
		
		// 이동 바인딩 (Axis)
		if (MoveAction)
		{
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &UExRunnerInputComponent::NativeOnMoveAction);
		}
	}
}

void UExRunnerInputComponent::NativeOnJumpAction(const FInputActionValue& Value)
{
	bool bIsPressed = Value.Get<bool>();
	OnJumpRequested.Broadcast(bIsPressed);
}

void UExRunnerInputComponent::NativeOnSlideAction(const FInputActionValue& Value)
{
	bool bIsPressed = Value.Get<bool>();
	OnSlideRequested.Broadcast(bIsPressed);
}

void UExRunnerInputComponent::NativeOnMoveAction(const FInputActionValue& Value)
{
	float AxisValue = Value.Get<float>();
	OnMoveRequested.Broadcast(AxisValue);
}

void UExRunnerInputComponent::RequestJumpAction(bool bIsTriggered)
{
	OnJumpRequested.Broadcast(bIsTriggered);
}

void UExRunnerInputComponent::RequestSlideAction(bool bIsTriggered)
{
	OnSlideRequested.Broadcast(bIsTriggered);
}

void UExRunnerInputComponent::RequestMoveAction(float AxisValue)
{
	OnMoveRequested.Broadcast(AxisValue);
}
