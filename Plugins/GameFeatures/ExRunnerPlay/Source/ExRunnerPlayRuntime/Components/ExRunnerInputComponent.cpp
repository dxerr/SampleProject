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

		// 스프린트 바인딩 (체크박스 지속/해제 처리에 적합하도록 Triggered와 Completed 바인딩)
		if (SprintAction)
		{
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Triggered, this, &UExRunnerInputComponent::NativeOnSprintAction);
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &UExRunnerInputComponent::NativeOnSprintAction);
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

void UExRunnerInputComponent::NativeOnSprintAction(const FInputActionValue& Value)
{
	bool bIsPressed = Value.Get<bool>();
	OnSprintRequested.Broadcast(bIsPressed);
}

void UExRunnerInputComponent::NativeOnMoveAction(const FInputActionValue& Value)
{
	float AxisValue = Value.Get<float>();
	OnMoveRequested.Broadcast(AxisValue);
}

void UExRunnerInputComponent::RequestJumpAction(bool bIsTriggered)
{
	InjectInputBoolForAction(JumpAction, bIsTriggered);
}

void UExRunnerInputComponent::RequestSlideAction(bool bIsTriggered)
{
	InjectInputBoolForAction(SlideAction, bIsTriggered);
}

void UExRunnerInputComponent::RequestSprintAction(bool bIsTriggered)
{
	InjectInputBoolForAction(SprintAction, bIsTriggered);
}

void UExRunnerInputComponent::RequestMoveAction(float AxisValue)
{
	InjectInputFloatForAction(MoveAction, AxisValue);
}
