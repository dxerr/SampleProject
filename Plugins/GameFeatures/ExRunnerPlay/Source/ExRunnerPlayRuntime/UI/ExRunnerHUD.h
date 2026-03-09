// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "UI/Widgets/ExHUDLayoutWidget.h"
#include "ExRunnerHUD.generated.h"

/**
 * 러너 게임 전용 기본 HUD 클래스
 * 게임 시작 시 GameMode에 의해 생성되며, 
 * 설정된 HUDLayoutClass 위젯을 자동으로 화면에 띄웁니다.
 */
UCLASS()
class EXRUNNERPLAYRUNTIME_API AExRunnerHUD : public AHUD
{
	GENERATED_BODY()

public:
	AExRunnerHUD();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 화면에 띄울 HUD Layout 베이스 위젯 클래스 (블루프린트에서 UExRunnerHUDLayout 파생 클래스 지정) */
	UPROPERTY(EditDefaultsOnly, Category = "ExUI")
	TSubclassOf<UExHUDLayoutWidget> HUDLayoutClass;

private:
	/** 생성되어 화면에 떠있는 레이아웃 위젯의 포인터 기억 */
	UPROPERTY(Transient)
	TObjectPtr<UExHUDLayoutWidget> SpawnedHUDLayout;
};
