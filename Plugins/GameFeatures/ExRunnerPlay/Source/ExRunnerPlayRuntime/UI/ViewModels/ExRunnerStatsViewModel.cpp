// Fill out your copyright notice in the Description page of Project Settings.

#include "ExRunnerStatsViewModel.h"
#include "../../Components/ExRunnerStatComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "UObject/UObjectIterator.h"

void UExRunnerStatsViewModel::AutoInitialize(APlayerController* InController)
{
	if (!InController) return;

	APawn* OwnerPawn = InController->GetPawn();
	if (!OwnerPawn)
	{
		// 멀티플레이 및 늦은 스폰 시 Pawn이 아직 없을 수 있으므로 델리게이트 바인딩
		InController->OnPossessedPawnChanged.AddDynamic(this, &UExRunnerStatsViewModel::OnPawnPossessed);
		UE_LOG(LogTemp, Warning, TEXT("[ExRunnerStatsViewModel] AutoInitialize: Pawn is not yet possessed. Waiting..."));
		return;
	}

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

	// 3순위: World 전체 컴포넌트를 순회하며 Controller의 Pawn과 매칭되는지 확인 (최후의 보루)
	if (UWorld* World = InController->GetWorld())
	{
		for (TObjectIterator<UExRunnerStatComponent> It; It; ++It)
		{
			if (It->GetWorld() == World && It->GetOwner() == OwnerPawn)
			{
				UE_LOG(LogTemp, Warning, TEXT("[ExRunnerStatsViewModel] AutoInitialize: TObjectIterator Fallback으로 ExRunnerStatComponent를 찾았습니다!"));
				InitializeRunnerBindings(*It);
				return;
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[ExRunnerStatsViewModel] AutoInitialize: ExRunnerStatComponent를 최종적으로 찾지 못했습니다. Pawn '%s'"), *OwnerPawn->GetName());
}

void UExRunnerStatsViewModel::OnPawnPossessed(APawn* OldPawn, APawn* NewPawn)
{
	if (NewPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ExRunnerStatsViewModel] Pawn possessed: %s. Retrying initialization."), *NewPawn->GetName());
		if (APlayerController* PC = Cast<APlayerController>(NewPawn->GetController()))
		{
			// 제거하여 중복 호출 방지
			PC->OnPossessedPawnChanged.RemoveDynamic(this, &UExRunnerStatsViewModel::OnPawnPossessed);
			AutoInitialize(PC);
		}
	}
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

void UExRunnerStatsViewModel::SetCurrentDistance(float NewDistance)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(CurrentDistance, NewDistance))
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetCurrentDistanceText);
	}
}

void UExRunnerStatsViewModel::SetCoinCount(int32 NewCount)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(CoinCount, NewCount))
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetCoinCountText);
	}
}

void UExRunnerStatsViewModel::SetSprintRemainingTime(float NewTime)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(SprintRemainingTime, NewTime))
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetSprintRemainingTimeText);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetSprintProgress);
	}
}

void UExRunnerStatsViewModel::SetMaxSprintTime(float NewMaxTime)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(MaxSprintTime, NewMaxTime))
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetSprintProgress);
	}
}

float UExRunnerStatsViewModel::GetSprintProgress() const
{
	if (MaxSprintTime <= 0.0f)
	{
		return 0.0f;
	}
	// 0.0 ~ 1.0 비율 반환
	return FMath::Clamp(SprintRemainingTime / MaxSprintTime, 0.0f, 1.0f);
}

void UExRunnerStatsViewModel::InitializeRunnerBindings(UExRunnerStatComponent* InStatComponent)
{
	if (!InStatComponent)
		return;

	BoundStatComponent = InStatComponent;

	// 1. 초기값 즉시 반영 (SetCurrentSpeed가 Broadcast까지 처리)
	SetCurrentSpeed(BoundStatComponent->GetCurrentRunningSpeed());
	SetCoinCount(BoundStatComponent->GetCoinCount());
	SetCurrentDistance(BoundStatComponent->GetCurrentDistance());
	SetSprintRemainingTime(BoundStatComponent->GetSprintRemainingTime());
	SetMaxSprintTime(1.0f); // Default safe value

	// 2. 값 변경 이벤트를 구독(Bind)하여 ViewModel 갱신 연결
	BoundStatComponent->OnRunnerSpeedChanged.AddDynamic(this, &UExRunnerStatsViewModel::OnSpeedUpdated);
	BoundStatComponent->OnCoinCountChanged.AddDynamic(this, &UExRunnerStatsViewModel::OnCoinCountUpdated);
	BoundStatComponent->OnRunnerDistanceChanged.AddDynamic(this, &UExRunnerStatsViewModel::OnDistanceUpdated);
	BoundStatComponent->OnSprintTimeChanged.AddDynamic(this, &UExRunnerStatsViewModel::OnSprintTimeUpdated);
}

void UExRunnerStatsViewModel::OnSpeedUpdated(float NewSpeed)
{
	// SetCurrentSpeed 내부가 UE_MVVM_SET_PROPERTY_VALUE로 처리되므로
	// 이벤트 기반 값 갱신 + UI Broadcast가 자동으로 수행됩니다.
	SetCurrentSpeed(NewSpeed);
}

void UExRunnerStatsViewModel::OnCoinCountUpdated(int32 NewCount)
{
	SetCoinCount(NewCount);
}

void UExRunnerStatsViewModel::OnDistanceUpdated(float NewDistance)
{
	SetCurrentDistance(NewDistance);
}

void UExRunnerStatsViewModel::OnSprintTimeUpdated(float RemainingTime)
{
	// 타이머가 새로 시작되었을 경우 (이전 시간이 0 초과 상태가 아니었음) MaxTime 업데이트
	if (RemainingTime > 0.0f && SprintRemainingTime <= 0.0f)
	{
		SetMaxSprintTime(RemainingTime);
	}
	SetSprintRemainingTime(RemainingTime);
}

FText UExRunnerStatsViewModel::GetCurrentSpeedText() const
{
	// 소수점 1자리까지 표시 (예: "123.4")
	return FText::AsNumber(CurrentSpeed, &FNumberFormattingOptions::DefaultNoGrouping());
}

FText UExRunnerStatsViewModel::GetCoinCountText() const
{
	// 기본 숫자 포맷 표시 (천 단위 구분 기호 포함, "1,000")
	return FText::AsNumber(CoinCount);
}

FText UExRunnerStatsViewModel::GetCurrentDistanceText() const
{
	// cm를 m로 변환 (소수점 없이)
	float DistM = CurrentDistance / 100.0f;
	FNumberFormattingOptions Opts;
	Opts.MaximumFractionalDigits = 0;
	Opts.MinimumFractionalDigits = 0;
	return FText::FromString(FString::Printf(TEXT("%s m"), *FText::AsNumber(DistM, &Opts).ToString()));
}

FText UExRunnerStatsViewModel::GetSprintRemainingTimeText() const
{
	FNumberFormattingOptions Opts;
	Opts.MinimumFractionalDigits = 1;
	Opts.MaximumFractionalDigits = 1;
	return FText::AsNumber(SprintRemainingTime, &Opts);
}
