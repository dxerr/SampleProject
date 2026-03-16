// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ExBaseTouchPadWidget.generated.h"

/**
 * 범용적인 터치패드/가상 조이스틱을 위한 기본 Widet 클래스입니다.
 * 
 * 디스플레이 요소(BgImage, ThumbImage)는 WBP에서 배치하고 이 클래스를 상속받아 연동합니다.
 * 복잡한 터치 위치 연산, 썸네일 이동(가운데 정렬), 정규화 처리를 C++에서 처리하여 WBP 확장을 용이하게 합니다.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class EXCORERUNTIME_API UExBaseTouchPadWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 터치 패드 배경 위젯 (WBP에서 BgImage라는 이름으로 캔버스 슬롯 등에 배치하면 바인딩됨)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UWidget> BgImage;

	// 터치 포인터를 따라다니는 썸네일 위젯 (WBP에서 ThumbImage라는 이름으로 배치하면 바인딩됨)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UWidget> ThumbImage;

	// 에디터에서 설정 가능한 ThumbImage의 크기 (정중앙 렌더 트랜스레이션을 위해 사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExUI|TouchPad")
	FVector2D ThumbSize = FVector2D(50.f, 50.f);

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;

	// 터치 시작 시 블루프린트로 알림
	UFUNCTION(BlueprintImplementableEvent, Category = "ExUI|TouchPad", meta = (DisplayName = "On Touch Pad Started"))
	void BP_OnTouchPadStarted(FVector2D LocalPosition);

	// 터치 이동 시 블루프린트로 델타, 로컬위치, 정규화된 오프셋(-1.0 ~ 1.0)을 모두 계산하여 알림
	UFUNCTION(BlueprintImplementableEvent, Category = "ExUI|TouchPad", meta = (DisplayName = "On Touch Pad Moved"))
	void BP_OnTouchPadMoved(FVector2D OutFrameDelta, FVector2D OutLocalPosition, FVector2D OutNormalizedOffset);

	// 터치 종료 시 블루프린트로 알림
	UFUNCTION(BlueprintImplementableEvent, Category = "ExUI|TouchPad", meta = (DisplayName = "On Touch Pad Ended"))
	void BP_OnTouchPadEnded();

private:
	// 내부 상태 관리 변수
	bool bIsTouching = false;
	int32 TouchPointerIndex = -1;
	FVector2D LastTouchPosition = FVector2D::ZeroVector;

	// 터치 위치에 맞게 ThumbImage의 RenderTranslation을 정중앙으로 업데이트
	void UpdateThumbPosition(const FVector2D& LocalPosition, const FVector2D& PadSize);
	
	// 터치가 끝났을 때 ThumbImage를 원래 자리(중앙)로 복구 (옵션)
	void ResetThumbPosition();
};
