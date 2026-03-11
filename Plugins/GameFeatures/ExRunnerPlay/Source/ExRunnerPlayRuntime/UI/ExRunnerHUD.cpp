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
		UE_LOG(LogExRunnerHUD, Log, TEXT("AExRunnerHUD: Experience 시스템이 HUD 렌더링을 담당하므로 HUDLayoutClass 기반 자체 생성 로직을 건너뜁니다. -> %s"), *HUDLayoutClass->GetName());
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
