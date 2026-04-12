// Copyright ExFrameWork. All Rights Reserved.

#include "ExRunnerFadeOverlayWidget.h"
#include "UI/Subsystems/ExUIManagerSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Animation/WidgetAnimation.h"

DEFINE_LOG_CATEGORY_STATIC(LogFadeOverlay, Log, All);

void UExRunnerFadeOverlayWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// UIManagerSubsystem 캐싱
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			CachedUIMgr = LP->GetSubsystem<UExUIManagerSubsystem>();
		}
	}
}

void UExRunnerFadeOverlayWidget::PlayFadeIn(float Duration)
{
	UE_LOG(LogFadeOverlay, Log, TEXT("[FadeOverlay] PlayFadeIn 호출 — Duration=%.2f"), Duration);

	if (FadeInAnim)
	{
		// 애니메이션 종료 델리게이트 바인딩
		FWidgetAnimationDynamicEvent EndEvent;
		EndEvent.BindDynamic(this, &UExRunnerFadeOverlayWidget::OnFadeInFinished);
		BindToAnimationFinished(FadeInAnim, EndEvent);

		// 애니메이션 재생 (Duration에 맞춰 속도 조절)
		// UMG 애니메이션 기본 지속 시간 대비 비율 계산
		float AnimTotalTime = FadeInAnim->GetEndTime();
		float PlayRate = (Duration > 0.0f) ? (AnimTotalTime / Duration) : 1.0f;

		PlayAnimation(FadeInAnim, 0.0f, 1, EUMGSequencePlayMode::Forward, PlayRate);
	}
	else
	{
		UE_LOG(LogFadeOverlay, Warning, TEXT("[FadeOverlay] FadeInAnim 애니메이션이 바인딩되지 않았습니다. (BP에서 이름 확인 필요)"));
		// 애니메이션이 없으면 즉시 다음 단계 진행 (폴백)
		OnFadeComplete();
	}
}

void UExRunnerFadeOverlayWidget::OnFadeInFinished()
{
	UE_LOG(LogFadeOverlay, Log, TEXT("[FadeOverlay] FadeIn 애니메이션 종료"));
	
	// 애니메이션 종료 후 델리게이트 해제 (애니메이션에 등록된 이 오브젝트의 모든 델리게이트 제거)
	UnbindAllFromAnimationFinished(FadeInAnim);

	// 재시작 팝업 표시 트리거
	OnFadeComplete();
}

void UExRunnerFadeOverlayWidget::OnFadeComplete_Implementation()
{
	UE_LOG(LogFadeOverlay, Log, TEXT("[FadeOverlay] OnFadeComplete — 재시작 팝업 표시"));

	if (!CachedUIMgr)
	{
		UE_LOG(LogFadeOverlay, Warning, TEXT("[FadeOverlay] UIManagerSubsystem이 없습니다. NativeConstruct에서 캐싱 실패."));
		return;
	}

	// FOnExPopupResultNative — DECLARE_DELEGATE_TwoParams(FOnExPopupResultNative, EExModalResult, const FText&)
	CachedUIMgr->ShowConfirm(
		NSLOCTEXT("ExRunner", "GameOver", "게임 오버"),
		NSLOCTEXT("ExRunner", "RestartPrompt", "다시 시작하시겠습니까?"),
		FOnExPopupResultNative::CreateUObject(this, &UExRunnerFadeOverlayWidget::HandleRestartResult)
	);
}

void UExRunnerFadeOverlayWidget::HandleRestartResult(EExModalResult Result, const FText& InputText)
{
	if (Result == EExModalResult::Confirm)
	{
		UE_LOG(LogFadeOverlay, Log, TEXT("[FadeOverlay] 재시작 선택 — RequestRestartMatch"));
		// GameMode에 재시작 요청
		if (APlayerController* PC = GetOwningPlayer())
		{
			PC->RestartLevel();
		}
	}
	else
	{
		UE_LOG(LogFadeOverlay, Log, TEXT("[FadeOverlay] 취소 선택 — ReturnToLobby"));
		// TODO: GameMode::ReturnToLobby 연결 (GameFlowSubsystem 활용)
	}

	// 오버레이 제거
	DeactivateWidget();
}
