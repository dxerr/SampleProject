// Copyright ExFrameWork. All Rights Reserved.

#include "ExRunnerHUD.h"
#include "Blueprint/UserWidget.h"
#include "Engine/LocalPlayer.h"

DEFINE_LOG_CATEGORY_STATIC(LogExRunnerHUD, Log, All);

AExRunnerHUD::AExRunnerHUD()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AExRunnerHUD::BeginPlay()
{
	Super::BeginPlay();

	// Local Player를 가져옵니다 (서버나 봇인 경우 UI를 안 띄움)
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC || !PC->IsLocalPlayerController())
	{
		return;
	}

	if (HUDLayoutClass)
	{
		// CommonUI 기반 위젯이므로 CreateWidget 후 루트 캔버스에 추가
		SpawnedHUDLayout = CreateWidget<UExHUDLayoutWidget>(PC, HUDLayoutClass);
		if (SpawnedHUDLayout)
		{
			SpawnedHUDLayout->AddToViewport();
			UE_LOG(LogExRunnerHUD, Log, TEXT("AExRunnerHUD: %s 레이아웃이 화면에 성공적으로 추가되었습니다."), *HUDLayoutClass->GetName());
		}
	}
	else
	{
		UE_LOG(LogExRunnerHUD, Warning, TEXT("AExRunnerHUD: HUDLayoutClass가 블루프린트에서 설정되지 않았습니다!"));
	}
}

void AExRunnerHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SpawnedHUDLayout)
	{
		SpawnedHUDLayout->RemoveFromParent();
		SpawnedHUDLayout = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}
