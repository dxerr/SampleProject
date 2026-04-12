// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "UI/Widgets/ExPopupWidget.h"
#include "ExRunnerFadeOverlayWidget.generated.h"

class UExUIManagerSubsystem;
class UWidgetAnimation;

/**
 * UExRunnerFadeOverlayWidget
 * 낙하 사망(FallDeath) 전용 페이드아웃 오버레이 위젯
 *
 * 배치: GameStack (게임 오버레이 레이어 — 입력 차단 없음)
 *
 * 흐름:
 *   OnGameOverReasonUpdated(FallDeath)
 *     → UIManagerSubsystem->PushGameOverlay(FadeOverlayWidget)
 *          → PlayFadeIn(1.5f)   ← 화면 서서히 어두워짐
 *               → OnFadeComplete() [BP 호출]
 *                    → ShowConfirm()
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class EXRUNNERPLAYRUNTIME_API UExRunnerFadeOverlayWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/**
	 * ViewModel이 PushGameOverlay() 후 즉시 호출
	 * @param Duration 페이드인 지속 시간 (초)
	 */
	UFUNCTION(BlueprintCallable, Category = "ExUI|FadeOverlay")
	void PlayFadeIn(float Duration = 1.5f);

	/**
	 * 페이드 완료 시 BP에서 호출 → 재시작 팝업 표시
	 * BP에서는 이 함수 이후 ShowConfirm이 자동으로 호출됨
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ExUI|FadeOverlay")
	void OnFadeComplete();
	virtual void OnFadeComplete_Implementation();

protected:
	virtual void NativeConstruct() override;

private:
	/** 팝업 결과 콜백 — DECLARE_DELEGATE_TwoParams(FOnExPopupResultNative, EExModalResult, const FText&) */
	void HandleRestartResult(EExModalResult Result, const FText& InputText);

	/** UIManagerSubsystem 캐싱 */
	UPROPERTY()
	TObjectPtr<UExUIManagerSubsystem> CachedUIMgr;

	/** 
	 * [자동 연동] BP에서 위젯 애니메이션의 이름을 'FadeInAnim'으로 만들면 자동으로 바인딩됩니다.
	 * PlayFadeIn() 호출 시 이 애니메이션이 자동 재생됩니다.
	 */
	UPROPERTY(Transient, meta = (BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> FadeInAnim;

	/** 애니메이션 종료 시 자동 호출될 콜백 */
	UFUNCTION()
	void OnFadeInFinished();
};
