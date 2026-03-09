// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/ExHUDLayoutWidget.h"
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
};
