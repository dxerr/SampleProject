// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/ExHUDLayoutWidget.h"
#include "InputStrategies/ExRunnerInputMode.h"
#include "ExRunnerHUDLayout.generated.h"

/**
 * 러너 게임 전용 HUD 레이아웃 베이스 (GameStack / MenuStack 관리)
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class EXRUNNERPLAYRUNTIME_API UExRunnerHUDLayout : public UExHUDLayoutWidget
{
	GENERATED_BODY()

public:
	// 러너 게임에 특화된 입력 설정 (Game Only, 추가적인 터치/마우스 설정 등)
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// WBP에서 InputPadSwitcher 라는 이름으로 위젯 스위처를 배치하면 자동으로 바인딩됩니다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UWidgetSwitcher> InputPadSwitcher;

	// 터치패드용 스위처 인덱스 (기본값 0)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ExUI|Input")
	int32 TouchPadWidgetIndex = 0;

	// 기본 버튼패드용 스위처 인덱스 (기본값 1)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ExUI|Input")
	int32 ButtonPadWidgetIndex = 1;

private:
	// 입력 모드 변경을 수신하는 내부 콜백
	UFUNCTION()
	void HandleInputModeChanged(EExRunnerInputMode NewInputMode);

	// 바인딩 성공 여부 플래그
	bool bIsInputBound = false;
};
