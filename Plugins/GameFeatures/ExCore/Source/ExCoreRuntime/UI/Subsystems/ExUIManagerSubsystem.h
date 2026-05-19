// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "CommonActivatableWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "GameplayTagContainer.h"
#include "UI/Data/ExUIDataAsset.h"
#include "UI/Data/ExPopupDescriptor.h"
#include "UI/Data/ExToastDescriptor.h"
#include "UI/Widgets/ExPopupWidget.h"
#include "UI/Widgets/ExToastWidget.h"
#include "ExUIManagerSubsystem.generated.h"

// Forward declarations for specific widget types
class UExWindowWidget;
class UExModalWidget;

/**
 * ExCore의 전역 UI 스택 매니저입니다.
 * LocalPlayer 화면(Viewport)마다 독립적으로 유지되며, 
 * HUD Layout으로부터 스택(GameStack, MenuStack)을 주입받아
 * Window 타입(인벤토리 등)이나 Modal 패턴(확인 안내창) 등 
 * 각종 위젯의 Push / Pop 라우팅을 총괄합니다.
 */
UCLASS()
class EXCORERUNTIME_API UExUIManagerSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * HUD Layout 위젯이 생성될 때 자신이 가진 스택들을 매니저에 등록합니다.
	 * 
	 * @param InGameStack 미니맵, 퀘스트 등 게임 오버레이용 (ZOrder 하위)
	 * @param InMenuStack 인벤토리, 타이틀메뉴, 팝업 등 인터랙티브 UI용 (ZOrder 상위)
	 */
	UFUNCTION(BlueprintCallable, Category = "ExUI")
	void RegisterStacks(UCommonActivatableWidgetStack* InGameStack, UCommonActivatableWidgetStack* InMenuStack);

	/**
	 * HUD Layout이 사용할 팝업 위젯 클래스를 등록합니다.
	 * ExHUDLayoutWidget 파생 위젯의 디테일 패널에서 설정하면 자동으로 호출됩니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ExUI|Classes")
	void SetPopupWidgetClass(TSubclassOf<UExPopupWidget> InClass);

	/**
	 * HUD Layout이 사용할 토스트 위젯 클래스를 등록합니다.
	 * ExHUDLayoutWidget 파생 위젯의 디테일 패널에서 설정하면 자동으로 호출됩니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ExUI|Classes")
	void SetToastWidgetClass(TSubclassOf<UExToastWidget> InClass);


	/**
	 * 창 모드 기반 위젯(UExWindowWidget 파생)을 Menu Stack에 추가(Push)합니다.
	 * 추가됨과 동시에 위젯은 자동으로 Activate 되며 입력을 가져갑니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ExUI")
	UCommonActivatableWidget* PushWindow(TSubclassOf<UExWindowWidget> WidgetClass);

	/**
	 * 팝업 대화상자 기반 위젯(UExModalWidget 파생)을 Menu Stack 최상단에 추가합니다.
	 * (Modal 모드에 따라 하위 위젯 입력 차단 등은 위젯 내부 InputConfig를 따름)
	 */
	UFUNCTION(BlueprintCallable, Category = "ExUI")
	UCommonActivatableWidget* PushModal(TSubclassOf<UExModalWidget> WidgetClass);

	/**
	 * 인게임 오버레이 전용 위젯(미니맵 등)을 Game Stack에 추가합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ExUI")
	UCommonActivatableWidget* PushGameOverlay(TSubclassOf<UCommonActivatableWidget> WidgetClass);

	/**
	 * Menu Stack의 최상단 위젯(활성화된 탭/팝업 등)을 스택에서 제거(Pop)합니다.
	 * CommonUI가 포커스를 자동으로 계산해 직전 위젯에 반환합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ExUI")
	void PopMenu();

	/**
	 * 특정 위젯 인스턴스를 찾아 스택에서 명시적으로 제거합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ExUI")
	void RemoveWidget(UCommonActivatableWidget* Widget);

	// --- 팝업 (Popup) API ---

	// C++ 전용 편의 함수 (람다 전달 용)
	void ShowInfo(const FText& Title, const FText& Body, float AutoCloseSeconds = 3.0f);
	void ShowAcknowledge(const FText& Title, const FText& Body, FOnExPopupResultNative OnResult = nullptr);
	void ShowConfirm(const FText& Title, const FText& Body, FOnExPopupResultNative OnResult);
	void ShowInputPrompt(const FText& Title, const FText& Body, const FExPopupInputDesc& InputConfig, FOnExPopupResultNative OnResult);
	void ShowPopup(const FExPopupDescriptor& Descriptor, FOnExPopupResultNative OnResult = nullptr);

	// BP 전용 편의 함수 (반환된 위젯의 OnPopupResult 바인딩 용)
	UFUNCTION(BlueprintCallable, Category="ExUI|Popup", meta=(DisplayName="Show Info Popup"))
	UExPopupWidget* ShowInfoBP(const FText& Title, const FText& Body, float AutoCloseSeconds = 3.0f);

	UFUNCTION(BlueprintCallable, Category="ExUI|Popup", meta=(DisplayName="Show Acknowledge Popup"))
	UExPopupWidget* ShowAcknowledgeBP(const FText& Title, const FText& Body);

	UFUNCTION(BlueprintCallable, Category="ExUI|Popup", meta=(DisplayName="Show Confirm Popup"))
	UExPopupWidget* ShowConfirmBP(const FText& Title, const FText& Body);

	UFUNCTION(BlueprintCallable, Category="ExUI|Popup", meta=(DisplayName="Show Input Prompt Popup"))
	UExPopupWidget* ShowInputPromptBP(const FText& Title, const FText& Body, const FExPopupInputDesc& InputConfig);

	UFUNCTION(BlueprintCallable, Category="ExUI|Popup", meta=(DisplayName="Show Popup (Advanced)"))
	UExPopupWidget* ShowPopupBP(const FExPopupDescriptor& Descriptor);

	// --- 토스트 (Toast) API ---

	UFUNCTION(BlueprintCallable, Category="ExUI|Toast")
	void RegisterToastContainer(UPanelWidget* InToastContainer);

	UFUNCTION(BlueprintCallable, Category="ExUI|Toast")
	UExToastWidget* ShowToast(const FText& Message, float Duration = 3.0f);

	UFUNCTION(BlueprintCallable, Category="ExUI|Toast")
	UExToastWidget* ShowTimedToast(const FText& Message, float Duration = 3.0f);

	UFUNCTION(BlueprintCallable, Category="ExUI|Toast")
	UExToastWidget* ShowLoadingToast(const FText& Message);

	UFUNCTION(BlueprintCallable, Category="ExUI|Toast")
	UExToastWidget* ShowToastFromDescriptor(const FExToastDescriptor& Descriptor);

	/** 현재 화면에 떠 있는 모든 토스트를 즉시 또는 애니메이션과 함께 강제 종료합니다. */
	UFUNCTION(BlueprintCallable, Category="ExUI|Toast")
	void ClearAllToasts(bool bForceImmediate = false);

	void HandleToastClosed(UExToastWidget* ClosedToast);

	// --- 데이터 드리븐(Data-Driven) UI 제어 API ---

	/**
	 * 싱글톤/지연 초기화 패턴: GameFeature 활성화 시 바로 로드하지 않고
	 * 대기열에 등록만 해둡니다. 실제 UI 호출 시점에 로딩됩니다.
	 */
	static void AddPendingUIData(const TSoftObjectPtr<UExUIDataAsset>& SoftUIData);
	static void RemovePendingUIData(const TSoftObjectPtr<UExUIDataAsset>& SoftUIData);

	/**
	 * DataAsset을 통해 UI 매핑 정보를 글로벌 레지스트리에 등록합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ExUI")
	void RegisterUIData(const UExUIDataAsset* UIData);

	/**
	 * 플러그인 등 해제 시 글로벌 레지스트리에서 UI 매핑 정보를 제거합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ExUI")
	void UnregisterUIData(const UExUIDataAsset* UIData);

	/**
	 * 태그를 통해 UI를 특정 스택에 활성화시킵니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ExUI")
	UCommonActivatableWidget* PushUIByTag(FGameplayTag UITag);

	/**
	 * 태그를 통해 활성화된 해당 UI를 찾아 스택에서 제거합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ExUI")
	void PopUIByTag(FGameplayTag UITag);

private:
	/** 태그로 UI를 찾기 전, 대기 중인 데이터를 실제로 로드하여 레지스트리에 병합합니다. */
	void ProcessPendingUIData();

	/** 전역 대기열 (모든 GameFeature 액션들이 공유) */
	static TArray<TSoftObjectPtr<UExUIDataAsset>> PendingUIDataList;

	/** 이 로컬 플레이어 서브시스템에서 이미 로드처리를 완료한 대상 캐싱 */
	UPROPERTY(Transient)
	TSet<TObjectPtr<const UExUIDataAsset>> LoadedUIDataAssets;

	/** 현재 런타임에 등록된 모든 UI 매핑 데이터 */
	UPROPERTY(Transient)
	TMap<FGameplayTag, FExUIEntry> GlobalUIActiveRegistry;

	/** 태그로 띄운 활성화된 위젯 객체들의 레퍼런스 트래킹용 */
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UCommonActivatableWidget>> ActivatedWidgetsByTag;
	/** HUD 레이아웃에서 등록된 게임 플레이 층 스택 포인터 */
	UPROPERTY(Transient)
	TObjectPtr<UCommonActivatableWidgetStack> GameStack;

	/** HUD 레이아웃에서 등록된 메뉴/팝업 층 스택 포인터 */
	UPROPERTY(Transient)
	TObjectPtr<UCommonActivatableWidgetStack> MenuStack;

	// --- 팝업 & 토스트 속성 ---

	UPROPERTY(BlueprintReadWrite, Category="ExUI|Classes", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UExPopupWidget> PopupWidgetClass;

	UPROPERTY(BlueprintReadWrite, Category="ExUI|Classes", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UExToastWidget> ToastWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UPanelWidget> ToastContainer;

	// 서브시스템은 추적만 하므로 약참조 배열 (GC 개입 X)
	TArray<TWeakObjectPtr<UExToastWidget>> ActiveToasts;

	UPROPERTY()
	int32 MaxVisibleToasts = 3;

	// 화면 노출 한계 치 초과율 방어용 큐 (이탈 메시지 방지)
	TArray<FExToastDescriptor> PendingToastsQueue;
};
