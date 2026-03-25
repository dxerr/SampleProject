// Fill out your copyright notice in the Description page of Project Settings.


#include "ExBaseTouchPadWidget.h"
#include "Components/Widget.h"
#include "Blueprint/WidgetLayoutLibrary.h"

void UExBaseTouchPadWidget::NativeConstruct()
{
	Super::NativeConstruct();

#if WITH_EDITOR
	// 에디터 환경에서 마우스 클릭 시 포커스를 획득하여 캡처 유실 방지
	SetIsFocusable(true);
#endif

	bIsTouching = false;
	TouchPointerIndex = -1;
	LastTouchPosition = FVector2D::ZeroVector;

	// 초기 상태에서 썸네일을 리셋 (원래 자리로)
	ResetThumbPosition();
}

FReply UExBaseTouchPadWidget::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	if (bIsTouching)
	{
		return FReply::Unhandled();
	}

	bIsTouching = true;
	TouchPointerIndex = InGestureEvent.GetPointerIndex();

	// AbsoluteToLocal 변환을 통해 캔버스 내부 기준 좌상단(0,0) 좌표계 획득
	FVector2D LocalPosition = InGeometry.AbsoluteToLocal(InGestureEvent.GetScreenSpacePosition());
	
	LastTouchPosition = LocalPosition;

	// 썸네일 이동 처리
	UpdateThumbPosition(LocalPosition, InGeometry.GetLocalSize());

	// BP 이벤트 브로드캐스트
	BP_OnTouchPadStarted(LocalPosition);

	// 이벤트 핸들링 완료 처리 및 마우스 캡처 (위젯 밖으로 나가도 터치 추적 유지)
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UExBaseTouchPadWidget::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	// 현재 터치 중이면서, 최초에 등록된 터치 인덱스인 경우에만 로직 수행 (멀티터치 간섭 차단)
	if (bIsTouching && TouchPointerIndex == InGestureEvent.GetPointerIndex())
	{
		FVector2D CurrentLocalPosition = InGeometry.AbsoluteToLocal(InGestureEvent.GetScreenSpacePosition());
		
		// 1. 프레임간 델타 이동량 (캐릭터 좌우 회전 등에 사용)
		FVector2D FrameDelta = CurrentLocalPosition - LastTouchPosition;
		
		// 2. 값 정규화 프로세스 (-1.0 ~ 1.0)
		FVector2D SectionSize = InGeometry.GetLocalSize();
		
		// 디바이드바이제로 방지 및 크기 기반 센터 도출
		FVector2D Center = FVector2D::ZeroVector;
		FVector2D NormalizedOffset = FVector2D::ZeroVector;

		if (SectionSize.X > 0.f && SectionSize.Y > 0.f)
		{
			Center = SectionSize * 0.5f;
			FVector2D CenterOffset = CurrentLocalPosition - Center;
			
			// 패드 크기를 반경으로 삼아 값을 -1.0에서 1.0 사이로 Clamping하며 노멀라이즈
			NormalizedOffset.X = FMath::Clamp(CenterOffset.X / Center.X, -1.0f, 1.0f);
			NormalizedOffset.Y = FMath::Clamp(CenterOffset.Y / Center.Y, -1.0f, 1.0f);
		}

		// 상태 갱신
		LastTouchPosition = CurrentLocalPosition;

		// 썸네일 이동 처리
		UpdateThumbPosition(CurrentLocalPosition, InGeometry.GetLocalSize());

		// 통합된 값을 BP로 전달 (ViewModel로 바로 연결하기 위함)
		BP_OnTouchPadMoved(FrameDelta, CurrentLocalPosition, NormalizedOffset);

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply UExBaseTouchPadWidget::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	if (bIsTouching && TouchPointerIndex == InGestureEvent.GetPointerIndex())
	{
		bIsTouching = false;
		TouchPointerIndex = -1;
		LastTouchPosition = FVector2D::ZeroVector;

		// 터치가 끝나면 썸네일을 리셋
		ResetThumbPosition();

		// BP 이벤트 호출
		BP_OnTouchPadEnded();

		// 핸들링 완료 및 마우스 캡처 해제
		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

#if WITH_EDITOR
FReply UExBaseTouchPadWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	UE_LOG(LogTemp, Warning, TEXT("[ExTouchPad] MouseDown - Index: %d, Pos: %s"), InMouseEvent.GetPointerIndex(), *InMouseEvent.GetScreenSpacePosition().ToString());
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton || InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		// C++ 터치 로직 실행
		NativeOnTouchStarted(InGeometry, InMouseEvent);
		
		// CommonUI나 다른 서브시스템이 마우스 포커스를 뺏어 캡처가 풀리는 것을 방지하기 위해 강제로 Focus 지정
		return FReply::Handled().CaptureMouse(TakeWidget()).SetUserFocus(TakeWidget(), EFocusCause::Mouse);
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UExBaseTouchPadWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsTouching && InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ExTouchPad] MouseMove - Pos: %s"), *InMouseEvent.GetScreenSpacePosition().ToString());
		return NativeOnTouchMoved(InGeometry, InMouseEvent);
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UExBaseTouchPadWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	UE_LOG(LogTemp, Warning, TEXT("[ExTouchPad] MouseUp - Index: %d"), InMouseEvent.GetPointerIndex());
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		return NativeOnTouchEnded(InGeometry, InMouseEvent);
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UExBaseTouchPadWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
	if (bIsTouching)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ExTouchPad] MouseCaptureLost!!"));
		bIsTouching = false;
		TouchPointerIndex = -1;
		LastTouchPosition = FVector2D::ZeroVector;
		ResetThumbPosition();
		BP_OnTouchPadEnded();
	}
}
#endif

void UExBaseTouchPadWidget::UpdateThumbPosition(const FVector2D& LocalPosition, const FVector2D& PadSize)
{
	if (ThumbImage)
	{
		// 앵커(0.5,0.5)와 얼라인먼트(0.5,0.5)를 사용하면 썸네일의 기본 렌더 위치는 패드의 정중앙입니다.
		// 터치한 위치(LocalPosition)로 이동시키려면, 패드 중앙점(Center)과의 오프셋만 Translation에 반영해주면 됩니다.
		FVector2D Center = PadSize * 0.5f;
		FVector2D TargetTranslation = LocalPosition - Center;
		ThumbImage->SetRenderTranslation(TargetTranslation);
	}
}

void UExBaseTouchPadWidget::ResetThumbPosition()
{
	if (ThumbImage)
	{
		ThumbImage->SetRenderTranslation(FVector2D::ZeroVector);
	}
}
