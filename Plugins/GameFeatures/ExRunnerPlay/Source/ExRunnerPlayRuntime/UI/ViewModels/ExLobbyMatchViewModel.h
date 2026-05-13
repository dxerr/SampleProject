// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "UI/Widgets/ExModalWidget.h"   // EExModalResult
#include "ExLobbyMatchViewModel.generated.h"

class UExOnlineSubsystem;
class UExPopupWidget;
class UExUIManagerSubsystem;
class ULocalPlayer;

/**
 * UExLobbyMatchViewModel
 *
 * 로비 화면에서 멀티플레이 Quick Match 흐름을 전담하는 ViewModel.
 * 
 * 책임:
 *  - ExOnlineSubsystem::FindQuickMatch / CancelMatch 호출
 *  - 매칭 대기 팝업(Acknowledge) 생성 및 관리
 *  - 매칭 성공/실패 결과 팝업(Info) 표시
 *  - 중복 호출 방지 (bIsMatching 상태 관리)
 *
 * View(WBP_LobbyList)에서 필요한 호출:
 *  1. OnActivated → AutoInitialize(Self)
 *  2. MultiPlay 버튼 On Clicked → StartQuickMatch()
 */
UCLASS(BlueprintType, Blueprintable)
class EXRUNNERPLAYRUNTIME_API UExLobbyMatchViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/**
	 * View의 OnActivated 시점에 1회 호출.
	 * ExOnlineSubsystem 참조를 캐싱하고 초기 상태를 설정합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ExLobby|Match", meta = (WorldContext = "WorldContextObject"))
	void AutoInitialize(UObject* WorldContextObject);

	/**
	 * MultiPlay 버튼 On Clicked에 연결할 단일 함수.
	 * 매칭 대기 팝업 표시 → FindQuickMatch 호출 → 결과 처리까지 내부에서 수행.
	 * 매칭 진행 중 재호출 시 무시됩니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ExLobby|Match")
	void StartQuickMatch();

private:
	// ── ExOnlineSubsystem 콜백 ────────────────────────────────────────

	/** FindQuickMatch 결과 수신 */
	UFUNCTION()
	void OnMatchFoundCallback(bool bSuccess, const FString& ErrorMessage);

	/** 팝업(대기 팝업)의 버튼 클릭 결과 수신 — 취소 처리 */
	UFUNCTION()
	void OnMatchingPopupResult(EExModalResult Result, const FText& InputText);

	/** EOS 로그인 완료 콜백 — 로그인 완료 전 버튼 클릭 대비 */
	UFUNCTION()
	void OnLoginCompleteCallback(bool bSuccess, const FString& ErrorMessage);

	// ── 내부 헬퍼 ─────────────────────────────────────────────────────

	/** 매칭 대기 팝업 표시 (Acknowledge: 취소 버튼 1개) */
	void ShowMatchingPopup();

	/** 현재 대기 팝업 닫기 */
	void CloseMatchingPopup();

	/** 매칭 결과 Info 팝업 표시 (AutoClose) */
	void ShowResultPopup(const FText& Title, const FText& Body);

	/** UIManagerSubsystem 안전 반환 */
	class UExUIManagerSubsystem* GetUIManager() const;

	// ── 상태 ──────────────────────────────────────────────────────────

	/** 매칭 진행 중 여부 — 중복 호출 방지 */
	bool bIsMatching = false;

	/** 현재 표시 중인 대기 팝업 (닫기용 캐싱) */
	UPROPERTY(Transient)
	TObjectPtr<UExPopupWidget> ActiveMatchingPopup;

	/** 캐싱된 ExOnlineSubsystem (BeginPlay 이후 유효) */
	UPROPERTY(Transient)
	TObjectPtr<UExOnlineSubsystem> CachedOnlineSubsystem;

	/** AutoInitialize 시 Widget->GetOwningLocalPlayer()로 얻은 LocalPlayer 캐싱 — UIManager 접근용 */
	TWeakObjectPtr<ULocalPlayer> CachedLocalPlayer;

	/** 매칭 Config (MatchMode, MaxPlayers 고정값) */
	static constexpr int32 DefaultMaxPlayers = 2;
	static const FString DefaultMatchMode;
};
