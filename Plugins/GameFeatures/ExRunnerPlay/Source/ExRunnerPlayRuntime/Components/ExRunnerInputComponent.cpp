#include "ExRunnerInputComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "InputAction.h"
#include "Util/Actor/ExActorUtil.h"
#include "Data/ExRunnerConfig.h"
#include "Subsystems/ExDataCenterSubsystem.h"
#include "GameFramework/Pawn.h"
#include "Debug/ExDebugDrawSubsystem.h"
#include "Tags/ExGameplayTags.h"
#include "InputStrategies/ExRunnerInputStrategy.h"
#include "InputStrategies/ExRunnerInputStrategy_Manual.h"
#include "InputStrategies/ExRunnerInputStrategy_AutoRun.h"
#include "InputStrategies/ExRunnerInputStrategy_AutoButtonRun.h"
#include "ExRunnerMovementComponent.h"
#include "Engine/World.h"
#include "ExRunnerPlayRuntimeModule.h"



void UExRunnerInputComponent::BeginPlay()
{
	Super::BeginPlay();

	UGameInstance* GI = GetWorld()->GetGameInstance();
	if (!GI) return;

	UExDataCenterSubsystem* DC = GI->GetSubsystem<UExDataCenterSubsystem>();
	if (!DC) return;

	// Config가 이미 로드되어 있으면 즉시 초기화
	if (UExRunnerConfig* Config = DC->GetConfig<UExRunnerConfig>())
	{
		CachedConfig = Config;
		ApplyInputMode(CachedConfig->Input.DefaultInputMode);
	}
	else
	{
		// [수정] OnDataLoaded -> OnDataCenterUpdated (서브시스템 실제 멤버명)
		DC->OnDataCenterUpdated.AddDynamic(this, &UExRunnerInputComponent::OnDataCenterUpdated);
	}
}

void UExRunnerInputComponent::OnDataCenterUpdated()
{
	if (UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		if (UExDataCenterSubsystem* DC = GI->GetSubsystem<UExDataCenterSubsystem>())
		{
			if (UExRunnerConfig* Config = DC->GetConfig<UExRunnerConfig>())
			{
				CachedConfig = Config;
				ApplyInputMode(CachedConfig->Input.DefaultInputMode);
				UE_LOG(LogExRunnerPlay, Log, TEXT("UExRunnerInputComponent: Config loaded/updated, applying DefaultInputMode"));
			}
		}
	}
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

	if (GEngine) GEngine->AddOnScreenDebugMessage(84, 0.0f, FColor::Purple, FString::Printf(TEXT("[InputComp] OnMoveAction Received: X=%.2f, Y=%.2f"), AxisValue.X, AxisValue.Y));


	// Strategy에 좌우 입력 위임
	// Manual 모드: OnMoveRequested.Broadcast
	// AutoRun 모드: OnLaneChangeRequested 스냅 판정
	if (ActiveStrategy)
	{
		ActiveStrategy->HandleHorizontalInput(AxisValue);
	}
	else
	{
		// Fallback: Strategy 미초기화 시 기존 동작 유지
		OnMoveRequested.Broadcast(AxisValue);
	}
}

void UExRunnerInputComponent::RequestJumpAction(bool bIsTriggered)
{
	// Strategy 게이트: AutoRun 모드에서는 쿨다운 중 차단
	if (ActiveStrategy && !ActiveStrategy->CanRequestJump(bIsTriggered))
	{
		return;
	}

	if (bIsTriggered)
	{
		// 점프 발동 직전, 곡선 궤적을 예측하여 방향을 미리 틀어줌 (관성 이탈 보정)
		if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
		{
			if (UExRunnerMovementComponent* MovComp = OwnerPawn->FindComponentByClass<UExRunnerMovementComponent>())
			{
				MovComp->ApplyPreJumpRotation();
			}
		}
	}

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
	// Strategy 게이트: AutoRun 모드에서는 쿨다운 중 차단
	if (ActiveStrategy && !ActiveStrategy->CanRequestSlide(bIsTriggered))
	{
		return;
	}

	InjectInputBoolForAction(SlideAction, bIsTriggered);

	if (!bIsTriggered)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ExRunnerInput] RequestSlideAction(false) → OnSlideRequested.Broadcast(false) 직접 호출"));
		OnSlideRequested.Broadcast(false);
	}
}

void UExRunnerInputComponent::RequestSprintAction(bool bIsTriggered)
{
	// Strategy 게이트: 필요시 AutoRun 모드에서 스프린트 제한 가능
	if (ActiveStrategy && !ActiveStrategy->CanRequestSprint(bIsTriggered))
	{
		return;
	}
	InjectInputBoolForAction(SprintAction, bIsTriggered);
}

void UExRunnerInputComponent::RequestMoveAction(FVector2D AxisValue)
{
	if (AxisValue.SizeSquared() > 0.0f)
	{
		// 로그 스팸 방지를 위해 주석 처리
		// UE_LOG(LogTemp, Warning, TEXT("[InputInjection] MoveAction: %s"), *AxisValue.ToString());
	}
	InjectInputVectorForAction(MoveAction, FVector(AxisValue, 0.0f));
}



void UExRunnerInputComponent::RequestLookAction(float YawAxisValue)
{
	if (ActiveStrategy)
	{
		ActiveStrategy->HandleHorizontalInput(FVector2D(YawAxisValue, 0.0f));
	}
	else
	{
		// Fallback: Strategy가 아직 안 붙었다면 원래 하던 대로 Broadcast 허용
		OnLookRequested.Broadcast(YawAxisValue);
	}
}

void UExRunnerInputComponent::RequestLaneChange(int32 LaneDirection)
{
	if (ActiveStrategy)
	{
		ActiveStrategy->HandleLaneChangeRequest(LaneDirection);
	}
}

float UExRunnerInputComponent::GetSwipeActivationPercentage() const
{
	// 필수 애셋 누락 시 에디터 크래시(check)를 발생시켜 개발자가 즉시 인지하고 수정하도록 강제합니다. (가이드라인 1.7 준수)
	checkf(CachedConfig, TEXT("UExRunnerInputComponent: RunnerConfig가 로드되지 않았습니다. DataCenter 시스템을 확인해주세요."));

	if (CachedConfig)
	{
		return CachedConfig->Gameplay.SwipeActivationPercentage;
	}
	
	// Shipping 빌드 등 check가 무시되는 환경을 대비한 Fallback (30%)
	return 0.3f;
}

bool UExRunnerInputComponent::IsSlideInputActive() const
{
	// InjectedInputStates 맵에 SlideAction이 존재하면 슬라이드 입력이 현재 주입 중
	// ViewModel의 bIsSlideActive 등 별도 추적 변수 없이도 실제 컴포넌트 상태를 정확히 반영
	return SlideAction && InjectedInputStates.Contains(SlideAction);
}

void UExRunnerInputComponent::SetInputMode(EExRunnerInputMode NewMode)
{
	if (CurrentInputMode == NewMode) return; // 동일 모드 전환 무시
	ApplyInputMode(NewMode);
}

void UExRunnerInputComponent::RegisterMovementComponent(UExRunnerMovementComponent* InMovementComp)
{
	if (!InMovementComp) return;

	CachedMovementComp = InMovementComp;

	// 컴포넌트가 등록되는 시점에 이미 활성화된 Strategy가 있다면 즉시 바인딩 수행
	if (ActiveStrategy)
	{
		ActiveStrategy->BindToMovement(CachedMovementComp);
		UE_LOG(LogTemp, Log, TEXT("[ExRunnerInput] 지연 등록된 MovementComponent에 Strategy를 성공적으로 바인딩했습니다."));
	}
}

void UExRunnerInputComponent::ApplyInputMode(EExRunnerInputMode NewMode)
{
	// 1. 현재 Strategy가 있으면 MovementComponent 바인딩 해제
	if (ActiveStrategy)
	{
		if (CachedMovementComp)
		{
			ActiveStrategy->UnbindFromMovement(CachedMovementComp);
		}
		ActiveStrategy = nullptr;
	}

	// 2. None 모드: Strategy 없이 입력 무시
	if (NewMode == EExRunnerInputMode::None)
	{
		CurrentInputMode = NewMode;
		return;
	}

	// 3. 새 Strategy 인스턴스 생성
	TSubclassOf<UExRunnerInputStrategy> StrategyClass = nullptr;
	switch (NewMode)
	{
		case EExRunnerInputMode::Manual:
			StrategyClass = UExRunnerInputStrategy_Manual::StaticClass();
			break;
		case EExRunnerInputMode::AutoRun:
			StrategyClass = UExRunnerInputStrategy_AutoRun::StaticClass();
			break;
		case EExRunnerInputMode::AutoButtonRun:
			StrategyClass = UExRunnerInputStrategy_AutoButtonRun::StaticClass();
			break;
		default:
			break;
	}

	if (!StrategyClass) return;

	ActiveStrategy = NewObject<UExRunnerInputStrategy>(this, StrategyClass);
	ActiveStrategy->Initialize(this);

	// 4. MovementComponent에 모드별 델리게이트 바인딩
	if (CachedMovementComp)
	{
		ActiveStrategy->BindToMovement(CachedMovementComp);
	}
	else
	{
		// 이 로그는 정상입니다. 컨테이너 폰의 MovementComponent가 시각 폰 InputComponent보다 늦게 생성될 수 있습니다.
		// 차후 MovementComponent가 Timer를 거쳐 자신을 RegisterMovementComponent로 등록하면 그때 바인딩됩니다.
		UE_LOG(LogTemp, Log, TEXT("[ExRunnerInput] MovementComponent가 아직 식별되지 않았습니다. 대기 상태로 전환합니다."));
	}

	CurrentInputMode = NewMode;
	OnInputModeChanged.Broadcast(NewMode);
	UE_LOG(LogTemp, Log, TEXT("[ExRunnerInput] InputMode 전환 완료: %s"), *UEnum::GetValueAsString(NewMode));
}
