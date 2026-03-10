// Fill out your copyright notice in the Description page of Project Settings.

#include "Experience/ExExperienceManagerComponent.h"
#include "Experience/ExExperienceDefinition.h"
#include "Net/UnrealNetwork.h"
#include "Engine/LocalPlayer.h"
#include "UI/Subsystems/ExUIManagerSubsystem.h"
#include "UI/Widgets/ExHUDLayoutWidget.h"
#include "Kismet/GameplayStatics.h"


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
	if (bLoadComplete)
	{
		return;
	}

	// ---------------------------------------------------------
	// [TODO] 비동기 데이터 로딩 도입 위치
	// 현재는 TSubclassOf (하드 레퍼런스)로 에셋에 설정되어 있으므로 즉시 초기화됩니다.
	// 향후 TSoftClassPtr로 바인딩한다면 여기서 StreamableManager로 Async Load 후
	// 그 콜백에서 OnExperienceLoadComplete()를 호출하도록 구조 변경을 기획합니다.
	// ---------------------------------------------------------

	// 임시: 바로 완료 처리
	OnExperienceLoadComplete();
}

void UExExperienceManagerComponent::OnExperienceLoadComplete()
{
	bLoadComplete = true;

	// 로딩이 완전히 끝났음을 외부(GameMode 등)에 브로드캐스트
	OnExperienceLoadCompleteEvent.Broadcast();
}
