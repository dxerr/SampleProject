// Fill out your copyright notice in the Description page of Project Settings.

#include "ExRunnerStatsViewModel.h"
#include "../../Components/ExRunnerStatComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

void UExRunnerStatsViewModel::AutoInitialize(APlayerController* InController)
{
	if (!InController) return;

	APawn* OwnerPawn = InController->GetPawn();
	if (!OwnerPawn) return;

	// 1순위: Pawn 자신에게 직접 붙어있는 경우
	if (UExRunnerStatComponent* Direct = OwnerPawn->FindComponentByClass<UExRunnerStatComponent>())
	{
		InitializeRunnerBindings(Direct);
		return;
	}

	// 2순위: Pawn에 Attach된 Child Actor들 순회
	//        (SkeletalMesh 등 시각 전담 Actor에 StatComponent가 붙어있는 경우 대응)
	TArray<AActor*> AttachedActors;
	OwnerPawn->GetAttachedActors(AttachedActors, /*bResetArray=*/true, /*bRecursivelyIncludeAttachedActors=*/true);

	for (AActor* ChildActor : AttachedActors)
	{
		if (UExRunnerStatComponent* Found = ChildActor->FindComponentByClass<UExRunnerStatComponent>())
		{
			InitializeRunnerBindings(Found);
			return;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[ExRunnerStatsViewModel] AutoInitialize: ExRunnerStatComponent를 찾지 못했습니다. Pawn '%s' 및 하위 Actor들을 확인하세요."), *OwnerPawn->GetName());
}

void UExRunnerStatsViewModel::SetCurrentSpeed(float NewSpeed)
{
	// 값이 실제로 변경된 경우에만 Broadcast (최적화)
	// UE_MVVM_SET_PROPERTY_VALUE는 값 대입 + Broadcast를 한 번에 처리합니다.
	if (UE_MVVM_SET_PROPERTY_VALUE(CurrentSpeed, NewSpeed))
	{
		// CurrentSpeed가 변경되었으니 파생 계산값인 GetCurrentSpeedText도 갱신 알림
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetCurrentSpeedText);
	}
}

FText UExRunnerStatsViewModel::GetCurrentSpeedText() const
{
	// 소수점 1자리까지 표시 (예: "123.4")
	return FText::AsNumber(CurrentSpeed, &FNumberFormattingOptions::DefaultNoGrouping());
}

void UExRunnerStatsViewModel::InitializeRunnerBindings(UExRunnerStatComponent* InStatComponent)
{
	if (!InStatComponent)
		return;

	BoundStatComponent = InStatComponent;

	// 1. 초기값 즉시 반영 (SetCurrentSpeed가 Broadcast까지 처리)
	SetCurrentSpeed(BoundStatComponent->GetCurrentRunningSpeed());

	// 2. 값 변경 이벤트를 구독(Bind)하여 ViewModel 갱신 연결
	BoundStatComponent->OnRunnerSpeedChanged.AddDynamic(this, &UExRunnerStatsViewModel::OnSpeedUpdated);
}

void UExRunnerStatsViewModel::OnSpeedUpdated(float NewSpeed)
{
	// SetCurrentSpeed 내부가 UE_MVVM_SET_PROPERTY_VALUE로 처리되므로
	// 이벤트 기반 값 갱신 + UI Broadcast가 자동으로 수행됩니다.
	SetCurrentSpeed(NewSpeed);
}
