// Fill out your copyright notice in the Description page of Project Settings.

#include "ExRunnerMatchViewModel.h"
#include "UI/ViewModels/ExRunnerMatchViewModel.h"
#include "ExCoreSpawnDataAsset.h"
#include "GameModes/ExGameStateBase.h"
#include "GameStates/ExRunnerGameState.h"
#include "Tags/ExMatchTags.h"
#include "Experience/ExExperienceManagerComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "CommonAnimatedSwitcher.h"

DEFINE_LOG_CATEGORY_STATIC(LogExRunnerMatchVM, Log, All);

void UExRunnerMatchViewModel::AutoInitialize(UObject* WorldContextObject)
{
	if (!WorldContextObject) return;

	UWorld* World = WorldContextObject->GetWorld();
	if (!World) return;

	AExGameStateBase* GameState = World->GetGameState<AExGameStateBase>();
	if (!GameState)
	{
		UE_LOG(LogExRunnerMatchVM, Warning, TEXT("[ExRunnerMatchViewModel] AutoInitialize: AExGameStateBase를 찾을 수 없습니다."));
		return;
	}

	if (BoundGameState == GameState) 
	{
		UE_LOG(LogExRunnerMatchVM, Log, TEXT("[ExRunnerMatchViewModel] AutoInitialize: 이미 바인딩되어 있습니다."));
		return;
	}

	if (BoundGameState)
	{
		BoundGameState->OnMatchPhaseChanged.RemoveDynamic(this, &UExRunnerMatchViewModel::OnMatchPhaseUpdated);
	}

	BoundGameState = GameState;

	UpdateIndexByPhase(BoundGameState->GetCurrentMatchPhase());

	BoundGameState->OnMatchPhaseChanged.AddDynamic(this, &UExRunnerMatchViewModel::OnMatchPhaseUpdated);

	UE_LOG(LogExRunnerMatchVM, Log, TEXT("[ExRunnerMatchViewModel] AutoInitialize: 성공적으로 바인딩되었습니다. 초기 MatchPhase: %s"), *BoundGameState->GetCurrentMatchPhase().ToString());
}

void UExRunnerMatchViewModel::BindSwitcher(UCommonAnimatedSwitcher* InSwitcher)
{
	if (InSwitcher)
	{
		BoundSwitcher = InSwitcher;
		UE_LOG(LogExRunnerMatchVM, Log, TEXT("[ExRunnerMatchViewModel] 스위처 객체 할당 완료. 기존 보존상태 인덱스(%d) 반영."), ActiveWidgetIndex);
		BoundSwitcher->SetActiveWidgetIndex(ActiveWidgetIndex);
	}
}

void UExRunnerMatchViewModel::OnMatchPhaseUpdated(const FGameplayTag& OldPhase, const FGameplayTag& NewPhase)
{
	UE_LOG(LogExRunnerMatchVM, Log, TEXT("[ExRunnerMatchViewModel] OnMatchPhaseUpdated 콜백 발생! NewPhase: %s"), *NewPhase.ToString());
	UpdateIndexByPhase(NewPhase);
}

void UExRunnerMatchViewModel::UpdateIndexByPhase(const FGameplayTag& Phase)
{
	int32 NewIndex = 0; 

	if (Phase == ExMatchTags::Match_Playing)
	{
		NewIndex = 1;
	}
	else if (Phase == ExMatchTags::Match_PostMatch)
	{
		NewIndex = 2;
	}

	UE_LOG(LogExRunnerMatchVM, Log, TEXT("[ExRunnerMatchViewModel] UpdateIndexByPhase: 태그 %s 를 인덱스 %d 로 변환합니다."), *Phase.ToString(), NewIndex);
	SetActiveWidgetIndex(NewIndex);
}

void UExRunnerMatchViewModel::SetActiveWidgetIndex(int32 NewIndex)
{
	UE_LOG(LogExRunnerMatchVM, Log, TEXT("[ExRunnerMatchViewModel] SetActiveWidgetIndex 시도: %d"), NewIndex);
	
	if (UE_MVVM_SET_PROPERTY_VALUE(ActiveWidgetIndex, NewIndex))
	{
		UE_LOG(LogExRunnerMatchVM, Log, TEXT("[ExRunnerMatchViewModel] 인덱스가 변경되어 Broadcast 수행: %d"), NewIndex);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetActiveWidgetIndex);
		
		if (BoundSwitcher)
		{
			BoundSwitcher->SetActiveWidgetIndex(NewIndex);
			UE_LOG(LogExRunnerMatchVM, Log, TEXT("[ExRunnerMatchViewModel] ✅ 스위처 다이렉트 전환 명령 하달 완료: %d"), NewIndex);
		}
		else
		{
			UE_LOG(LogExRunnerMatchVM, Error, TEXT("[ExRunnerMatchViewModel] ❌ 스위처가 아직 바인딩되지 않아 강제 제어를 건너뜁니다. (BindSwitcher 호출 필요)"));
		}
	}
	else 
	{
		UE_LOG(LogExRunnerMatchVM, Log, TEXT("[ExRunnerMatchViewModel] 기존과 동일한 인덱스(%d)여서 무시됨."), NewIndex);
	}
}

int32 UExRunnerMatchViewModel::GetActiveWidgetIndex() const
{
	return ActiveWidgetIndex;
}
