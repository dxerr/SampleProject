// Fill out your copyright notice in the Description page of Project Settings.

#include "Experience/ExExperienceManagerComponent.h"
#include "Experience/ExExperienceDefinition.h"
#include "Net/UnrealNetwork.h"
#include "Engine/LocalPlayer.h"
#include "UI/Subsystems/ExUIManagerSubsystem.h"
#include "UI/Widgets/ExHUDLayoutWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/AssetManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogExExperience, Log, All);

// Sets default values for this component's properties
UExExperienceManagerComponent::UExExperienceManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}


// Called when the game starts
void UExExperienceManagerComponent::BeginPlay()
{
	Super::BeginPlay();
	// JIP(Join-in-Progress) 클라이언트를 위해, 이미 서버로부터 CurrentExperience를 
	// 넘겨받았다면 바로 로딩을 진행할 수 있도록 확인합니다.
	if (GetIsReplicated() && !HasAuthority())
	{
		if (CurrentExperience)
		{
			StartExperienceLoad();
		}
	}
}

void UExExperienceManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UExExperienceManagerComponent, CurrentExperience);
}

void UExExperienceManagerComponent::ServerSetCurrentExperience(const UExExperienceDefinition* InExperience)
{
	if (!HasAuthority())
	{
		return;
	}

	CurrentExperience = InExperience;

	// 서버 전용(Listen Server) 또는 에디터 테스트를 위해 서버 자신도 로드 진행
	if (CurrentExperience)
	{
		StartExperienceLoad();
	}
}

void UExExperienceManagerComponent::OnRep_CurrentExperience()
{
	// 서버가 새로운 Experience를 내려보냈을 때 클라이언트에서 불립니다.
	if (CurrentExperience)
	{
		StartExperienceLoad();
	}
}

void UExExperienceManagerComponent::StartExperienceLoad()
{
	if (bLoadComplete || !CurrentExperience)
	{
		return;
	}

	TArray<FSoftObjectPath> AssetPaths;

	// 하드 레퍼런스(TSubclassOf)에서 소프트 레퍼런스(TSoftClassPtr)로 변경됨에 따라 
	// AssetManager의 StreamableManager를 활용해 비동기 로딩을 수행합니다.
	if (CurrentExperience->DefaultHUDLayout.IsPending())
	{
		AssetPaths.AddUnique(CurrentExperience->DefaultHUDLayout.ToSoftObjectPath());
	}

	for (const TSoftClassPtr<UCommonActivatableWidget>& WidgetClass : CurrentExperience->ExtraWidgetsToLoad)
	{
		if (WidgetClass.IsPending())
		{
			AssetPaths.AddUnique(WidgetClass.ToSoftObjectPath());
		}
	}

	if (AssetPaths.Num() > 0)
	{
		UE_LOG(LogExExperience, Log, TEXT("[ExExperienceManagerComponent] 비동기 에셋 로딩을 시작합니다. 대상 개수: %d"), AssetPaths.Num());
		
		UAssetManager::GetStreamableManager().RequestAsyncLoad(
			AssetPaths,
			FStreamableDelegate::CreateUObject(this, &ThisClass::OnExperienceLoadComplete)
		);
	}
	else
	{
		UE_LOG(LogExExperience, Log, TEXT("[ExExperienceManagerComponent] 로딩할 에셋이 없거나 이미 로드되어 있습니다. 곧바로 완료 처리합니다."));
		OnExperienceLoadComplete();
	}
}

void UExExperienceManagerComponent::OnExperienceLoadComplete()
{
	bLoadComplete = true;

	UE_LOG(LogExExperience, Log, TEXT("[ExExperienceManagerComponent] Experience 로딩이 완료되었습니다. GameMode 등에 완료 브로드캐스트를 전송합니다."));
	
	// 로딩이 완전히 끝났음을 외부(GameMode 등)에 브로드캐스트
	OnExperienceLoadCompleteEvent.Broadcast();
}
