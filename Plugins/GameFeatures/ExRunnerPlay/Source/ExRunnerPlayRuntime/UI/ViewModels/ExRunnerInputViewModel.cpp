#include "ExRunnerInputViewModel.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "../Components/ExRunnerInputComponent.h"
#include "Debug/ExDebugDrawSubsystem.h"
#include "Tags/ExGameplayTags.h"

void UExRunnerInputViewModel::OnJumpButtonPressed()
{
	if (UExRunnerInputComponent* InputComp = GetRunnerInputComponent())
	{
		// 버튼을 누르는 순간 키보드 입력과 동일하게 True (눌림 판정)
		InputComp->RequestJumpAction(true);
	}
}

void UExRunnerInputViewModel::OnJumpButtonReleased()
{
	if (UExRunnerInputComponent* InputComp = GetRunnerInputComponent())
	{
		// 버튼에서 손을 뗄 때 False (뗌 판정 - 체공 시간 등 조절에 유효)
		InputComp->RequestJumpAction(false);
	}
}

void UExRunnerInputViewModel::OnSlideButtonPressed()
{
	if (UExRunnerInputComponent* InputComp = GetRunnerInputComponent())
	{
		InputComp->RequestSlideAction(true);
	}
}

void UExRunnerInputViewModel::OnSlideButtonReleased()
{
	if (UExRunnerInputComponent* InputComp = GetRunnerInputComponent())
	{
		InputComp->RequestSlideAction(false);
	}
}

void UExRunnerInputViewModel::OnSprintCheckStateChanged(bool bIsChecked)
{
	if (UExRunnerInputComponent* InputComp = GetRunnerInputComponent())
	{
		InputComp->RequestSprintAction(bIsChecked);
	}
}

void UExRunnerInputViewModel::OnTouchPadMoved(FVector2D FrameDelta, FVector2D LocalPosition, FVector2D NormalizedOffset)
{
	UExRunnerInputComponent* InputComp = GetRunnerInputComponent();
	if (!InputComp) return;

	// =========================================================
	// [X축] 좌우 드래그 → Yaw 회전 요청
	//   NormalizedOffset.X = 패드 중앙 기준 -1.0 ~ 1.0 (조이스틱 절대 위치)
	//   RunnerLookSensitivity 로 스케일되어 Yaw 델타로 변환됨
	// =========================================================
	if (!FMath::IsNearlyZero(NormalizedOffset.X))
	{
		InputComp->RequestLookAction(NormalizedOffset.X);
	}

	// =========================================================
	// [Y축] 상하 드래그 → 점프 / 슬라이드 스와이프
	//
	//  터치 '시작점' 기준 RelativeY 방식:
	//    - 첫 프레임에 NormY를 기준점(SwipeStartNormY)으로 저장
	//    - RelativeY = NormY(current) - SwipeStartNormY
	//    - 음수 → 위로(점프 방향), 양수 → 아래로(슬라이드 방향)
	//    - 터치 시작 위치와 무관하게 모두 SwipePct 만큼 실제로 스와이프해야 발동
	// =========================================================
	if (!bIsTouchPadActive)
	{
		// 터치 첫 프레임: 현재 NormY를 기준점으로 확정
		bIsTouchPadActive = true;
		SwipeStartNormY = NormalizedOffset.Y;
	}

	// 기준점 대비 현재 상대 이동량 (-1.0 ~ 1.0 범위)
	// 음수 = 시작점보다 위로 드래그 (= 점프)
	// 양수 = 시작점보다 아래로 드래그 (= 슬라이드)
	const float RelativeY = NormalizedOffset.Y - SwipeStartNormY;

	// DataSet에서 스와이프 임계 비율 조회
	const float SwipePct = InputComp->GetSwipeActivationPercentage();

	// 임계값 (정규화 RelativeY 기준)
	const float JumpThreshold  = -SwipePct;   // 위로 SwipePct만큼 슬라이드하면 점프
	const float SlideThreshold =  SwipePct;   // 아래로 SwipePct만큼 슬라이드하면 슬라이드

	// 디버그 로그 (출력 로그 사용)
	UE_LOG(LogTemp, Warning, TEXT("[ExTouchPadMoved] NormX: %.3f | NormY: %.3f | StartY: %.3f | RelY: %.3f | Thr: [%.3f / %.3f]"),
		NormalizedOffset.X, NormalizedOffset.Y, SwipeStartNormY, RelativeY, JumpThreshold, SlideThreshold);

	// 화면 디버그 텍스트
	if (UWorld* World = GetWorld())
	{
		if (UExDebugDrawSubsystem* DebugDraw = World->GetSubsystem<UExDebugDrawSubsystem>())
		{
			FString DebugMsg = FString::Printf(TEXT("[UI Swipe] NormX:%+.2f | RelY:%+.2f | Thr:[%.2f / %.2f]"),
				NormalizedOffset.X, RelativeY, JumpThreshold, SlideThreshold);
			DebugDraw->DrawScreenTextChecked(TAG_Ex_Debug_Speed, DebugMsg, FColor::Yellow, 0.0f);
		}
	}

	// 방향별 Handle 분기
	// - RelativeY < 0 (위): 점프 체크, 슬라이드 반드시 해제
	// - RelativeY > 0 (아래): 슬라이드 체크, 점프 반드시 해제
	// - RelativeY == 0 (제자리): 양쪽 모두 해제
	if (RelativeY < 0.0f)
	{
		HandleJumpInput(RelativeY, JumpThreshold);
		HandleSlideInput(0.0f, SlideThreshold);
	}
	else if (RelativeY > 0.0f)
	{
		HandleSlideInput(RelativeY, SlideThreshold);
		HandleJumpInput(0.0f, JumpThreshold);
	}
	else
	{
		HandleJumpInput(0.0f, JumpThreshold);
		HandleSlideInput(0.0f, SlideThreshold);
	}
}

void UExRunnerInputViewModel::OnTouchPadReleased()
{
	// 터치 상태 초기화
	bIsTouchPadActive = false;
	SwipeStartNormY = 0.0f;

	// [점프] HandleJumpInput(0, -9999): 0 <= -9999 불만족 → else 분기 → RequestJumpAction(false) + bIsJumpActive=false
	HandleJumpInput(0.0f, -9999.0f);

	// [슬라이드] Restore 재트리거 로직과 Release 정리는 분리해야 함.
	// HandleSlideInput(0, 9999)를 사용하면 Restore 분기(bCurrentlyInjected=true)에서 true를 재전송해버림.
	// → Release 시에는 InjectedInputStates에서 SlideAction을 제거하는 false를 직접 호출
	if (UExRunnerInputComponent* InputComp = GetRunnerInputComponent())
	{
		if (InputComp->IsSlideInputActive())
		{
			UE_LOG(LogTemp, Warning, TEXT("[ExSlide] *** RELEASE CLEANUP → false *** | InjectedInputStates에서 SlideAction 제거"));
			InputComp->RequestSlideAction(false);
		}
	}
}

bool UExRunnerInputViewModel::HandleJumpInput(float RelativeY, float Threshold)
{
	UExRunnerInputComponent* InputComp = GetRunnerInputComponent();
	if (!InputComp) return false;

	// [JUMP HOLD 방식] 슬라이드와 동일한 패턴
	// RelativeY <= Threshold (음수 임계를 넘어 위로 드래그된 상태) → 점프 유지
	// RelativeY > Threshold (임계값 미달 또는 복구) → 해제
	if (RelativeY <= Threshold)
	{
		if (!bIsJumpActive)
		{
			// 새로 임계값 돌파: True 1회 전송
			UE_LOG(LogTemp, Warning, TEXT("[ExJump] *** ACTIVATED *** | RelY: %.3f | Thr: %.3f | bIsJumpActive: false → true"),
				RelativeY, Threshold);
			InputComp->RequestJumpAction(true);
			bIsJumpActive = true;
			return true;
		}
		// 이미 활성 중이면 아무것도 안 함
		return false;
	}
	else
	{
		if (bIsJumpActive)
		{
			// 임계값 미달 또는 Release → False 전송
			UE_LOG(LogTemp, Warning, TEXT("[ExJump] *** DEACTIVATED *** | RelY: %.3f | Thr: %.3f | bIsJumpActive: true → false"),
				RelativeY, Threshold);
			InputComp->RequestJumpAction(false);
			bIsJumpActive = false;
		}
		return false;
	}
}

void UExRunnerInputViewModel::HandleSlideInput(float RelativeY, float Threshold)
{
	UExRunnerInputComponent* InputComp = GetRunnerInputComponent();
	if (!InputComp) return;

	// 상태 기준: 컴포넌트 내부 InjectedInputStates 직접 조회 (sync drift 방지)
	const bool bCurrentlyActive = InputComp->IsSlideInputActive();

	if (RelativeY >= Threshold)
	{
		// 조건 1: 임계값 이상 + 아직 비활성 → 활성화 (true 전송)
		if (!bCurrentlyActive)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ExSlide] *** ACTIVATE *** | RelY: %.3f >= Thr: %.3f | RequestSlideAction(true)"),
				RelativeY, Threshold);
			InputComp->RequestSlideAction(true);
		}
	}
	else
	{
		// 조건 2: 임계값 미달 + 현재 활성 중 → 복구 (false 전송)
		if (bCurrentlyActive)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ExSlide] *** RESTORE *** | RelY: %.3f < Thr: %.3f | RequestSlideAction(false)"),
				RelativeY, Threshold);
			InputComp->RequestSlideAction(false);
		}
	}
}


UExRunnerInputComponent* UExRunnerInputViewModel::GetRunnerInputComponent() const
{
	// 에디터/PIE 환경 내에서 유효한 첫 번째 로컬 플레이어를 찾아 폰(Pawn)의 컴포넌트를 가져옴
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				return Pawn->FindComponentByClass<UExRunnerInputComponent>();
			}
		}
	}
	return nullptr;
}
