// Fill out your copyright notice in the Description page of Project Settings.

#include "ExRunnerStatsViewModel.h"
#include "../../Components/ExRunnerStatComponent.h"
#include "../../Components/ExRunnerBuffComponent.h"
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

	// StatComponent 탐색 (기존 로직 유지)
	UExRunnerStatComponent* FoundStat = nullptr;

	// 1순위: Pawn 자신에게 직접 붙어있는 경우
	FoundStat = OwnerPawn->FindComponentByClass<UExRunnerStatComponent>();

	// 2순위: Pawn에 Attach된 Child Actor들 순회
	if (!FoundStat)
	{
		TArray<AActor*> AttachedActors;
		OwnerPawn->GetAttachedActors(AttachedActors, true, true);
		for (AActor* ChildActor : AttachedActors)
		{
			FoundStat = ChildActor->FindComponentByClass<UExRunnerStatComponent>();
			if (FoundStat) break;
		}
	}

	// 3순위: World 전체 순회 (Fallback)
	if (!FoundStat)
	{
		if (UWorld* World = InController->GetWorld())
		{
			for (TObjectIterator<UExRunnerStatComponent> It; It; ++It)
			{
				if (It->GetWorld() == World && It->GetOwner() == OwnerPawn)
				{
					UE_LOG(LogTemp, Warning, TEXT("[ExRunnerStatsViewModel] AutoInitialize: TObjectIterator Fallback으로 ExRunnerStatComponent를 찾았습니다!"));
					FoundStat = *It;
					break;
				}
			}
		}
	}

	if (FoundStat)
	{
		InitializeRunnerBindings(FoundStat);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ExRunnerStatsViewModel] AutoInitialize: ExRunnerStatComponent를 최종적으로 찾지 못했습니다. Pawn '%s'"), *OwnerPawn->GetName());
	}

	// BuffComponent도 같은 Pawn에서 탐색하여 바인딩
	// BP 노드 추가 없이 AutoInitialize 한 번으로 모두 처리됩니다.
	if (UExRunnerBuffComponent* BuffComp = OwnerPawn->FindComponentByClass<UExRunnerBuffComponent>())
	{
		InitializeBuffBindings(BuffComp);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ExRunnerStatsViewModel] AutoInitialize: ExRunnerBuffComponent를 찾지 못했습니다. 버프 UI가 표시되지 않습니다."));
	}
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

	// 1. 초기값 즉시 반영
	SetCurrentSpeed(BoundStatComponent->GetCurrentRunningSpeed());
	SetCoinCount(BoundStatComponent->GetCoinCount());
	SetCurrentDistance(BoundStatComponent->GetCurrentDistance());
	// 스프린트 잔여시간 초기값 = 0 (BuffComponent에서 관리)
	SetSprintRemainingTime(0.0f);
	SetMaxSprintTime(1.0f);

	// 2. 값 변경 이벤트 구독
	BoundStatComponent->OnRunnerSpeedChanged.AddDynamic(this, &UExRunnerStatsViewModel::OnSpeedUpdated);
	BoundStatComponent->OnCoinCountChanged.AddDynamic(this, &UExRunnerStatsViewModel::OnCoinCountUpdated);
	BoundStatComponent->OnRunnerDistanceChanged.AddDynamic(this, &UExRunnerStatsViewModel::OnDistanceUpdated);
}

void UExRunnerStatsViewModel::InitializeBuffBindings(UExRunnerBuffComponent* InBuffComponent)
{
	if (!InBuffComponent) return;

	BoundBuffComponent = InBuffComponent;

	// 버프 활성화/비활성화 이벤트 구독
	InBuffComponent->OnBuffActivated.AddDynamic(this,   &UExRunnerStatsViewModel::OnBuffActivatedUpdated);
	InBuffComponent->OnBuffDeactivated.AddDynamic(this,  &UExRunnerStatsViewModel::OnBuffDeactivatedUpdated);
	// 폴링 주기 잔여 시간 구독 — ProgressBar 카운트다운 용
	InBuffComponent->OnBuffTimeUpdated.AddDynamic(this,  &UExRunnerStatsViewModel::OnBuffTimeUpdated);
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

void UExRunnerStatsViewModel::OnBuffActivatedUpdated(EExBuffType BuffType, float Duration)
{
	// SpeedUp 버프 활성화 시에만 스프린트 UI 표시
	if (BuffType == EExBuffType::SpeedUp)
	{
		SetMaxSprintTime(Duration);
		SetSprintRemainingTime(Duration);
	}
}

void UExRunnerStatsViewModel::OnBuffDeactivatedUpdated(EExBuffType BuffType)
{
	if (BuffType == EExBuffType::SpeedUp)
	{
		SetSprintRemainingTime(0.0f);
	}
}

void UExRunnerStatsViewModel::OnBuffTimeUpdated(EExBuffType BuffType, float RemainingTime)
{
	// SpeedUp 버프의 잔여 시간을 폴링 주기마다 갱신 — ProgressBar 부드럽게 감소
	if (BuffType == EExBuffType::SpeedUp)
	{
		SetSprintRemainingTime(RemainingTime);
	}
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
