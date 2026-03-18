#include "ExRunnerInputComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "InputAction.h"
#include "Util/Actor/ExActorUtil.h"
#include "Data/Modes/ExGameModeDataSet.h"
#include "GameFramework/Pawn.h"
#include "Debug/ExDebugDrawSubsystem.h"
#include "Tags/ExGameplayTags.h"

void UExRunnerInputComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UExRunnerInputComponent::InitializeInputBindings(UEnhancedInputComponent* EnhancedInputComponent)
{
	Super::InitializeInputBindings(EnhancedInputComponent);

	if (EnhancedInputComponent)
	{
		// 점프 바인딩
		// true (Triggered/Started): Enhanced Input 주입 경로 → NativeOnJumpAction → Broadcast(true)
		// false: RequestJumpAction(false)에서 직접 Broadcast(false) 수행 (아래 함수 참고)
		if (JumpAction)
		{
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &UExRunnerInputComponent::NativeOnJumpAction);
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started,   this, &UExRunnerInputComponent::NativeOnJumpAction);
			// Completed 바인딩 제거: Completed 콜백에서는 Value.Get<bool>()이 true를 반환하는 Enhanced Input 스펙과 주입(false→true) 동일 프레임 패턴〈 혹은`}
		}

		// 슈라이드 바인딩
		// true (Triggered/Started): Enhanced Input 주입 경로 → NativeOnSlideAction → Broadcast(true)
		// false: RequestSlideAction(false)에서 직접 Broadcast(false) 수행 (아래 함수 참고)
		if (SlideAction)
		{
			EnhancedInputComponent->BindAction(SlideAction, ETriggerEvent::Triggered, this, &UExRunnerInputComponent::NativeOnSlideAction);
			EnhancedInputComponent->BindAction(SlideAction, ETriggerEvent::Started,   this, &UExRunnerInputComponent::NativeOnSlideAction);
			// Completed 바인딩 제거: false는 Request 함수에서 직접 처리
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
	FVector2D AxisValue = Value.Get<FVector2D>();
	if (AxisValue.SizeSquared() > 0.01f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[InputComp] NativeOnMoveAction: %s"), *AxisValue.ToString());
	}
	OnMoveRequested.Broadcast(AxisValue);
}

void UExRunnerInputComponent::RequestJumpAction(bool bIsTriggered)
{
	InjectInputBoolForAction(JumpAction, bIsTriggered);

	// [false 직접 Broadcast]
	// InjectInputBoolForAction(false) → InjectedInputStates.Remove → Enhanced Input Completed 발생
	// BUT: Completed 콜백의 Value.Get<bool>()은 마지막 활성 값(true)을 반환할 수 있어 불안정
	// → false 되는 순간 직접 Broadcast로 BP 이벤트를 확실히 전달
	if (!bIsTriggered)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ExRunnerInput] RequestJumpAction(false) → OnJumpRequested.Broadcast(false) 직접 호출"));
		OnJumpRequested.Broadcast(false);
	}
}

void UExRunnerInputComponent::RequestSlideAction(bool bIsTriggered)
{
	InjectInputBoolForAction(SlideAction, bIsTriggered);

	// [false 직접 Broadcast]
	// Enhanced Input Completed 콜백의 Value.Get<bool>()은 true를 반환하는 경우가 있어 불안정
	// (false → true 동일 프레임 패턴이나 Enhanced Input 스펙 문제)
	// → false 되는 순간 직접 Broadcast로 BP 이벤트를 확실히 전달
	if (!bIsTriggered)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ExRunnerInput] RequestSlideAction(false) → OnSlideRequested.Broadcast(false) 직접 호출"));
		OnSlideRequested.Broadcast(false);
	}
}

void UExRunnerInputComponent::RequestSprintAction(bool bIsTriggered)
{
	InjectInputBoolForAction(SprintAction, bIsTriggered);
}

void UExRunnerInputComponent::RequestMoveAction(FVector2D AxisValue)
{
	if (AxisValue.SizeSquared() > 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[InputInjection] MoveAction: %s"), *AxisValue.ToString());
	}
	InjectInputVectorForAction(MoveAction, FVector(AxisValue, 0.0f));
}



void UExRunnerInputComponent::RequestLookAction(float YawAxisValue)
{
	// NormX(-1.0 ~ 1.0) 값을 그대로 브로드캐스트합니다.
	// 실제 MaxRunnerYawAngle 곱셈 및 보간 처리는 ExRunnerMovementComponent::UpdateCharacterRotation에서 수행합니다.
	OnLookRequested.Broadcast(YawAxisValue);
}

float UExRunnerInputComponent::GetSwipeActivationPercentage() const
{
	// GameModeDataSet이 할당된 경우 DataSet 값 우선 반환
	if (GameModeDataSet)
	{
		return GameModeDataSet->SwipeActivationPercentage;
	}
	// DataSet 미할당 시 기본값(30%) 반환
	return 0.3f;
}

bool UExRunnerInputComponent::IsSlideInputActive() const
{
	// InjectedInputStates 맵에 SlideAction이 존재하면 슬라이드 입력이 현재 주입 중
	// ViewModel의 bIsSlideActive 등 별도 추적 변수 없이도 실제 컴포넌트 상태를 정확히 반영
	return SlideAction && InjectedInputStates.Contains(SlideAction);
}
