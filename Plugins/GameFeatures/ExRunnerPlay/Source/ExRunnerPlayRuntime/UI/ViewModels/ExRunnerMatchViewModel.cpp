// Fill out your copyright notice in the Description page of Project Settings.

#include "ExRunnerMatchViewModel.h"
#include "UI/ViewModels/ExRunnerMatchViewModel.h"
#include "ExCoreSpawnDataAsset.h"
#include "GameModes/ExGameStateBase.h"
#include "GameStates/ExRunnerGameState.h"
#include "Tags/ExMatchTags.h"
#include "Tags/ExRunnerTags.h"
#include "Experience/ExExperienceManagerComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "CommonAnimatedSwitcher.h"
#include "UI/Subsystems/ExUIManagerSubsystem.h"
#include "UI/Widgets/ExRunnerFadeOverlayWidget.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Player/ExRunnerPlayerState.h"

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

	// ── 룰 시스템 바인딩 ──────────────────────────────────────────
	if (AExRunnerGameState* RunnerGS = Cast<AExRunnerGameState>(GameState))
	{
		BoundRunnerGameState = RunnerGS;
		RunnerGS->OnRemainingTimeChanged.AddUObject(this, &UExRunnerMatchViewModel::OnRemainingTimeUpdated);
		RunnerGS->OnGameOverReasonChanged.AddUObject(this, &UExRunnerMatchViewModel::OnGameOverReasonUpdated);
		UE_LOG(LogExRunnerMatchVM, Log, TEXT("[ExRunnerMatchViewModel] 룰 시스템 델리게이트 바인딩 완료."));
	}

	// ── 로컬 플레이어 탈락 바인딩 (지연 바인딩) ───────────────────
	BindLocalPlayerState();

	UE_LOG(LogExRunnerMatchVM, Log, TEXT("[ExRunnerMatchViewModel] AutoInitialize: 성공적으로 바인딩되었습니다. 초기 MatchPhase: %s"), *BoundGameState->GetCurrentMatchPhase().ToString());
}

void UExRunnerMatchViewModel::BindLocalPlayerState()
{
	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (PC)
	{
		if (AExRunnerPlayerState* PS = PC->GetPlayerState<AExRunnerPlayerState>())
		{
			PS->OnLocalPlayerEliminated.AddUObject(this, &UExRunnerMatchViewModel::OnLocalPlayerEliminated);
			UE_LOG(LogExRunnerMatchVM, Log, TEXT("[ExRunnerMatchViewModel] 로컬 플레이어 탈락 이벤트 바인딩 완료."));
			return; // 성공 시 종료
		}
	}

	// 실패 시 0.5초 뒤 재시도
	FTimerHandle RetryHandle;
	World->GetTimerManager().SetTimer(RetryHandle, this, &UExRunnerMatchViewModel::BindLocalPlayerState, 0.5f, false);
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

// ── 룰 시스템 Getter ───────────────────────────────────────────────

float UExRunnerMatchViewModel::GetRemainingTime() const
{
	return RemainingTime;
}

bool UExRunnerMatchViewModel::GetIsTimerWarning() const
{
	return bIsTimerWarning;
}

EExRunnerGameOverReason UExRunnerMatchViewModel::GetGameOverReason() const
{
	return GameOverReason;
}

// ── 룰 시스템 콜백 ────────────────────────────────────────────────

void UExRunnerMatchViewModel::OnRemainingTimeUpdated(int32 NewSeconds)
{
	// 클라이언트가 수신한 정수 초를 float로 변환하여 로컬 보간 시작점으로 설정
	SetRemainingTime(static_cast<float>(NewSeconds));

	// WarningTime 기준은 Rule_Timer가 EventSubsystem 태그로 처리
	// 여기서는 GameState에 WarningTime 기준이 없으므로 직접 계산하지 않음
	UE_LOG(LogExRunnerMatchVM, Verbose, TEXT("[ExRunnerMatchVM] RemainingTime: %d초"), NewSeconds);
}

void UExRunnerMatchViewModel::OnGameOverReasonUpdated(EExRunnerGameOverReason Reason)
{
	SetGameOverReasonValue(Reason);

	// 전체 생존자가 사망하거나 시간이 종료되어 전역 게임 오버가 된 경우
	if (Reason != EExRunnerGameOverReason::None)
	{
		// 결과 화면 (PostMatch, index=2) 전환
		SetActiveWidgetIndex(2);
	}
}

void UExRunnerMatchViewModel::OnLocalPlayerEliminated(EExRunnerGameOverReason Reason)
{
	// 내(로컬)가 개별 탈락했을 때만 팝업 표시
	if (Reason == EExRunnerGameOverReason::FallDeath)
	{
		if (UWorld* World = GetWorld())
		{
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				if (ULocalPlayer* LP = PC->GetLocalPlayer())
				{
					if (UExUIManagerSubsystem* UIMgr = LP->GetSubsystem<UExUIManagerSubsystem>())
					{
						if (FadeOverlayWidgetClass)
						{
							UCommonActivatableWidget* Widget = UIMgr->PushGameOverlay(FadeOverlayWidgetClass);
							if (UExRunnerFadeOverlayWidget* FadeWidget = Cast<UExRunnerFadeOverlayWidget>(Widget))
							{
								FadeWidget->PlayFadeIn(1.5f);
							}
							UE_LOG(LogExRunnerMatchVM, Log, TEXT("[ExRunnerMatchVM] Local FallDeath — FadeOverlay 표시 완료"));
						}
						else
						{
							UE_LOG(LogExRunnerMatchVM, Warning, TEXT("[ExRunnerMatchVM] FallDeath — FadeOverlayWidgetClass가 설정되지 않았습니다. BP에서 연결 필요."));
						}
					}
				}
			}
		}
	}
}

// ── 룰 시스템 Setter ───────────────────────────────────────────────

void UExRunnerMatchViewModel::SetRemainingTime(float NewTime)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(RemainingTime, NewTime))
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetRemainingTime);
	}
}

void UExRunnerMatchViewModel::SetIsTimerWarning(bool bNewWarning)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(bIsTimerWarning, bNewWarning))
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetIsTimerWarning);
	}
}

void UExRunnerMatchViewModel::SetGameOverReasonValue(EExRunnerGameOverReason NewReason)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(GameOverReason, NewReason))
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetGameOverReason);
	}
}
