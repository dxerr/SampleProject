// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Subsystems/ExUIManagerSubsystem.h"
#include "UI/Widgets/ExWindowWidget.h"
#include "UI/Widgets/ExModalWidget.h"
#include "Engine/LocalPlayer.h"

// GameFeatureAction 등에서 지연(Lazy) 로드용으로 전역 공유하는 대기열
TArray<TSoftObjectPtr<UExUIDataAsset>> UExUIManagerSubsystem::PendingUIDataList;

void UExUIManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	GameStack = nullptr;
	MenuStack = nullptr;
}

void UExUIManagerSubsystem::Deinitialize()
{
	GameStack = nullptr;
	MenuStack = nullptr;
	Super::Deinitialize();
}

void UExUIManagerSubsystem::RegisterStacks(UCommonActivatableWidgetStack* InGameStack, UCommonActivatableWidgetStack* InMenuStack)
{
	GameStack = InGameStack;
	MenuStack = InMenuStack;

	if (GameStack && MenuStack)
	{
		UE_LOG(LogTemp, Log, TEXT("[ExUIManagerSubsystem] %s 로컬 플레이어의 UI 매니저에 스택들이 등록되었습니다."), *GetLocalPlayer()->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ExUIManagerSubsystem] 일부 UI 스택 등록이 누락되었습니다 (Game:%d, Menu:%d)"), InGameStack != nullptr, InMenuStack != nullptr);
	}
}

UCommonActivatableWidget* UExUIManagerSubsystem::PushWindow(TSubclassOf<UExWindowWidget> WidgetClass)
{
	if (!MenuStack)
	{
		UE_LOG(LogTemp, Error, TEXT("[ExUIManagerSubsystem] PushWindow 실패: MenuStack이 아직 등록되지 않았습니다."));
		return nullptr;
	}

	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ExUIManagerSubsystem] PushWindow 실패: 유효하지 않은 Window 클래스."));
		return nullptr;
	}

	// AddWidget은 자동으로 인스턴스화 후 ActivateWidget() 처리를 수행합니다.
	// 절대 활성화를 수동으로 재차 부르지 않아야 이중 활성 버그(입력 락킹 등)를 피할 수 있습니다.
	return MenuStack->AddWidget(WidgetClass);
}

UCommonActivatableWidget* UExUIManagerSubsystem::PushModal(TSubclassOf<UExModalWidget> WidgetClass)
{
	if (!MenuStack)
	{
		UE_LOG(LogTemp, Error, TEXT("[ExUIManagerSubsystem] PushModal 실패: MenuStack이 아직 등록되지 않았습니다."));
		return nullptr;
	}

	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ExUIManagerSubsystem] PushModal 실패: 유효하지 않은 Modal 클래스."));
		return nullptr;
	}

	return MenuStack->AddWidget(WidgetClass);
}

UCommonActivatableWidget* UExUIManagerSubsystem::PushGameOverlay(TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	if (!GameStack)
	{
		UE_LOG(LogTemp, Error, TEXT("[ExUIManagerSubsystem] PushGameOverlay 실패: GameStack이 아직 등록되지 않았습니다."));
		return nullptr;
	}

	if (!WidgetClass)
	{
		return nullptr;
	}

	return GameStack->AddWidget(WidgetClass);
}

void UExUIManagerSubsystem::PopMenu()
{
	if (MenuStack)
	{
		// 스택 포커스 정책 등에 따라 자식 스택이 알아서 순차 반환됩니다.
		// 활성화된 최상단 위젯(Active Widget)을 지우거나 Deactivate 시킵니다.
		UCommonActivatableWidget* TopWidget = MenuStack->GetActiveWidget();
		if (TopWidget)
		{
			// DeactivateWidget 시 스택에서 자연스럽게 빠져나가도록 UCommonActivatableWidget 쪽에 내부 세팅됨.
			TopWidget->DeactivateWidget();
		}
	}
}

void UExUIManagerSubsystem::RemoveWidget(UCommonActivatableWidget* Widget)
{
	if (!Widget) return;

	if (MenuStack)
	{
		MenuStack->RemoveWidget(*Widget);
	}
	
	if (GameStack)
	{
		GameStack->RemoveWidget(*Widget);
	}
}

// --- Data-Driven UI API Implementation ---

void UExUIManagerSubsystem::AddPendingUIData(const TSoftObjectPtr<UExUIDataAsset>& SoftUIData)
{
	PendingUIDataList.AddUnique(SoftUIData);
}

void UExUIManagerSubsystem::RemovePendingUIData(const TSoftObjectPtr<UExUIDataAsset>& SoftUIData)
{
	PendingUIDataList.Remove(SoftUIData);
}

void UExUIManagerSubsystem::ProcessPendingUIData()
{
	// 전역 대기열에 들어온 소프트 참조들을 순회하면서 현재 사용자의 시스템에 로드/등록해줍니다
	for (const TSoftObjectPtr<UExUIDataAsset>& SoftUIData : PendingUIDataList)
	{
		// 이전에 동기/비동기로 이미 로드된 적이 있는지 캐시 검사
		if (UExUIDataAsset* UIData = SoftUIData.LoadSynchronous())
		{
			if (!LoadedUIDataAssets.Contains(UIData))
			{
				RegisterUIData(UIData);
				LoadedUIDataAssets.Add(UIData);
			}
		}
	}
}

void UExUIManagerSubsystem::RegisterUIData(const UExUIDataAsset* UIData)
{
	if (!UIData) return;

	for (const auto& Pair : UIData->UIRegistry)
	{
		GlobalUIActiveRegistry.Add(Pair.Key, Pair.Value);
	}
	UE_LOG(LogTemp, Log, TEXT("[ExUIManagerSubsystem] %d개의 태그 기반 UI 데이터가 등록되었습니다."), UIData->UIRegistry.Num());
}

void UExUIManagerSubsystem::UnregisterUIData(const UExUIDataAsset* UIData)
{
	if (!UIData) return;

	for (const auto& Pair : UIData->UIRegistry)
	{
		GlobalUIActiveRegistry.Remove(Pair.Key);
	}
	UE_LOG(LogTemp, Log, TEXT("[ExUIManagerSubsystem] 태그 기반 UI 데이터가 해제되었습니다."));
}

UCommonActivatableWidget* UExUIManagerSubsystem::PushUIByTag(FGameplayTag UITag)
{
	if (!UITag.IsValid()) 
	{
		UE_LOG(LogTemp, Error, TEXT("[ExUIManagerSubsystem] PushUIByTag 실패: 전달된 태그가 유효하지 않습니다."));
		return nullptr;
	}

	// [Lazy Initialization] UI 호출 시점에 밀린 데이터 로드를 수행합니다.
	ProcessPendingUIData();

	if (const FExUIEntry* Entry = GlobalUIActiveRegistry.Find(UITag))
	{
		if (!Entry->WidgetClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ExUIManagerSubsystem] 태그 [%s]에 지정된 WidgetClass가 비어 있습니다."), *UITag.ToString());
			return nullptr;
		}

		UCommonActivatableWidget* SpawnedWidget = nullptr;
		switch (Entry->StackLayer)
		{
			case EExUIStackLayer::Game:
				if (GameStack) SpawnedWidget = GameStack->AddWidget(Entry->WidgetClass);
				break;
			case EExUIStackLayer::Menu:
				if (MenuStack) SpawnedWidget = MenuStack->AddWidget(Entry->WidgetClass);
				break;
			case EExUIStackLayer::Modal:
				if (MenuStack) SpawnedWidget = MenuStack->AddWidget(Entry->WidgetClass);
				break;
		}

		if (SpawnedWidget)
		{
			ActivatedWidgetsByTag.Add(UITag, SpawnedWidget);
			return SpawnedWidget;
		}
		
		UE_LOG(LogTemp, Error, TEXT("[ExUIManagerSubsystem] 태그 [%s] UI 생성 실패 (해당 스택이 존재하지 않을 가능성이 높습니다)."), *UITag.ToString());
		return nullptr;
	}

	UE_LOG(LogTemp, Error, TEXT("[ExUIManagerSubsystem] 글로벌 레지스트리에서 태그 [%s]에 해당하는 UI 맵핑 데이터를 찾을 수 없습니다."), *UITag.ToString());
	return nullptr;
}

void UExUIManagerSubsystem::PopUIByTag(FGameplayTag UITag)
{
	if (!UITag.IsValid()) return;

	if (TObjectPtr<UCommonActivatableWidget>* FoundWidget = ActivatedWidgetsByTag.Find(UITag))
	{
		if (UCommonActivatableWidget* WidgetToDeactivate = *FoundWidget)
		{
			WidgetToDeactivate->DeactivateWidget();
			RemoveWidget(WidgetToDeactivate);
		}
		ActivatedWidgetsByTag.Remove(UITag);
	}
}

// --- Popup API Implementation ---

void UExUIManagerSubsystem::ShowInfo(const FText& Title, const FText& Body, float AutoCloseSeconds)
{
	FExPopupDescriptor Desc;
	Desc.PopupType = EExPopupType::Info;
	Desc.Title = Title;
	Desc.Body = Body;
	Desc.AutoCloseSeconds = AutoCloseSeconds;
	ShowPopup(Desc, nullptr);
}

void UExUIManagerSubsystem::ShowAcknowledge(const FText& Title, const FText& Body, FOnExPopupResultNative OnResult)
{
	FExPopupDescriptor Desc;
	Desc.PopupType = EExPopupType::Acknowledge;
	Desc.Title = Title;
	Desc.Body = Body;
	
	FExPopupButtonDesc Btn;
	Btn.Label = FText::FromString(TEXT("확인")); // In real case, use LOCTEXT
	Btn.ResultValue = EExModalResult::Confirm;
	Desc.Buttons.Add(Btn);

	ShowPopup(Desc, OnResult);
}

void UExUIManagerSubsystem::ShowConfirm(const FText& Title, const FText& Body, FOnExPopupResultNative OnResult)
{
	FExPopupDescriptor Desc;
	Desc.PopupType = EExPopupType::Confirm;
	Desc.Title = Title;
	Desc.Body = Body;

	FExPopupButtonDesc BtnYes;
	BtnYes.Label = FText::FromString(TEXT("확인"));
	BtnYes.ResultValue = EExModalResult::Confirm;
	Desc.Buttons.Add(BtnYes);

	FExPopupButtonDesc BtnNo;
	BtnNo.Label = FText::FromString(TEXT("취소"));
	BtnNo.ResultValue = EExModalResult::Cancel;
	Desc.Buttons.Add(BtnNo);

	ShowPopup(Desc, OnResult);
}

void UExUIManagerSubsystem::ShowInputPrompt(const FText& Title, const FText& Body, const FExPopupInputDesc& InputConfig, FOnExPopupResultNative OnResult)
{
	FExPopupDescriptor Desc;
	Desc.PopupType = EExPopupType::InputPrompt;
	Desc.Title = Title;
	Desc.Body = Body;
	Desc.InputConfig = InputConfig;

	FExPopupButtonDesc BtnOk;
	BtnOk.Label = FText::FromString(TEXT("확인"));
	BtnOk.ResultValue = EExModalResult::Confirm;
	Desc.Buttons.Add(BtnOk);

	FExPopupButtonDesc BtnCancel;
	BtnCancel.Label = FText::FromString(TEXT("취소"));
	BtnCancel.ResultValue = EExModalResult::Cancel;
	Desc.Buttons.Add(BtnCancel);

	ShowPopup(Desc, OnResult);
}

void UExUIManagerSubsystem::ShowPopup(const FExPopupDescriptor& Descriptor, FOnExPopupResultNative OnResult)
{
	if (!PopupWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("PopupWidgetClass is not configured in UExUIManagerSubsystem."));
		return;
	}

	UCommonActivatableWidget* Widget = PushModal(PopupWidgetClass);
	if (UExPopupWidget* Popup = Cast<UExPopupWidget>(Widget))
	{
		// C++ Native Bind
		Popup->OnPopupResultNative = OnResult;
		Popup->InitFromDescriptor(Descriptor);
	}
}

// BP 
UExPopupWidget* UExUIManagerSubsystem::ShowInfoBP(const FText& Title, const FText& Body, float AutoCloseSeconds)
{
	FExPopupDescriptor Desc;
	Desc.PopupType = EExPopupType::Info;
	Desc.Title = Title;
	Desc.Body = Body;
	Desc.AutoCloseSeconds = AutoCloseSeconds;
	return ShowPopupBP(Desc);
}

UExPopupWidget* UExUIManagerSubsystem::ShowAcknowledgeBP(const FText& Title, const FText& Body)
{
	FExPopupDescriptor Desc;
	Desc.PopupType = EExPopupType::Acknowledge;
	Desc.Title = Title;
	Desc.Body = Body;
	
	FExPopupButtonDesc Btn;
	Btn.Label = FText::FromString(TEXT("OK"));
	Btn.ResultValue = EExModalResult::Confirm;
	Desc.Buttons.Add(Btn);

	return ShowPopupBP(Desc);
}

UExPopupWidget* UExUIManagerSubsystem::ShowConfirmBP(const FText& Title, const FText& Body)
{
	FExPopupDescriptor Desc;
	Desc.PopupType = EExPopupType::Confirm;
	Desc.Title = Title;
	Desc.Body = Body;

	FExPopupButtonDesc BtnYes;
	BtnYes.Label = FText::FromString(TEXT("Yes"));
	BtnYes.ResultValue = EExModalResult::Confirm;
	Desc.Buttons.Add(BtnYes);

	FExPopupButtonDesc BtnNo;
	BtnNo.Label = FText::FromString(TEXT("No"));
	BtnNo.ResultValue = EExModalResult::Cancel;
	Desc.Buttons.Add(BtnNo);

	return ShowPopupBP(Desc);
}

UExPopupWidget* UExUIManagerSubsystem::ShowInputPromptBP(const FText& Title, const FText& Body, const FExPopupInputDesc& InputConfig)
{
	FExPopupDescriptor Desc;
	Desc.PopupType = EExPopupType::InputPrompt;
	Desc.Title = Title;
	Desc.Body = Body;
	Desc.InputConfig = InputConfig;

	FExPopupButtonDesc BtnOk;
	BtnOk.Label = FText::FromString(TEXT("Confirm"));
	BtnOk.ResultValue = EExModalResult::Confirm;
	Desc.Buttons.Add(BtnOk);

	FExPopupButtonDesc BtnCancel;
	BtnCancel.Label = FText::FromString(TEXT("Cancel"));
	BtnCancel.ResultValue = EExModalResult::Cancel;
	Desc.Buttons.Add(BtnCancel);

	return ShowPopupBP(Desc);
}

UExPopupWidget* UExUIManagerSubsystem::ShowPopupBP(const FExPopupDescriptor& Descriptor)
{
	if (!PopupWidgetClass)
	{
		return nullptr;
	}

	UCommonActivatableWidget* Widget = PushModal(PopupWidgetClass);
	UExPopupWidget* Popup = Cast<UExPopupWidget>(Widget);
	if (Popup)
	{
		Popup->InitFromDescriptor(Descriptor);
	}
	return Popup;
}

// --- Toast API Implementation ---

void UExUIManagerSubsystem::RegisterToastContainer(UPanelWidget* InToastContainer)
{
	ToastContainer = InToastContainer;
	if (ToastContainer)
	{
		UE_LOG(LogTemp, Log, TEXT("[ExUIManagerSubsystem] ToastContainer Registered."));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ExUIManagerSubsystem] ToastContainer unregistered (nullptr)."));
		// Clean up valid active toasts if the container is gone
		// Note: The GC handles UWidgets, but we just clear our tracking.
		ActiveToasts.Empty();
		PendingToastsQueue.Empty();
	}
}

UExToastWidget* UExUIManagerSubsystem::ShowToastFromDescriptor(const FExToastDescriptor& Descriptor)
{
	if (!ToastContainer)
	{
		UE_LOG(LogTemp, Warning, TEXT("ShowToast failed: ToastContainer not registered."));
		return nullptr;
	}

	// Remove invalid pointers
	ActiveToasts.RemoveAll([](const TWeakObjectPtr<UExToastWidget>& Weak) { return !Weak.IsValid(); });

	// If at max capacity, add to pending queue and return.
	if (ActiveToasts.Num() >= MaxVisibleToasts)
	{
		PendingToastsQueue.Add(Descriptor);
		return nullptr; // We don't return the widget because it's not created yet
	}

	if (!ToastWidgetClass)
	{
		return nullptr;
	}

	UExToastWidget* Toast = CreateWidget<UExToastWidget>(GetLocalPlayer()->GetPlayerController(GetWorld()), ToastWidgetClass);
	if (Toast)
	{
		Toast->InitToast(Descriptor);
		Toast->OnToastClosed.BindUObject(this, &UExUIManagerSubsystem::HandleToastClosed);
		ToastContainer->AddChild(Toast);
		ActiveToasts.Add(Toast);
	}
	return Toast;
}

UExToastWidget* UExUIManagerSubsystem::ShowToast(const FText& Message, float Duration)
{
	FExToastDescriptor Desc;
	Desc.Message = Message;
	Desc.Duration = Duration;
	Desc.ProgressConfig.bShowProgressBar = false;
	Desc.ProgressConfig.bAutoCountdown = true; // 프로그레스 바는 숨기되 타이머는 흘러가서 자동 삭제되도록!
	return ShowToastFromDescriptor(Desc);
}

UExToastWidget* UExUIManagerSubsystem::ShowTimedToast(const FText& Message, float Duration)
{
	FExToastDescriptor Desc;
	Desc.Message = Message;
	Desc.Duration = Duration;
	Desc.ProgressConfig.bShowProgressBar = true;
	Desc.ProgressConfig.bAutoCountdown = true;
	return ShowToastFromDescriptor(Desc);
}

UExToastWidget* UExUIManagerSubsystem::ShowLoadingToast(const FText& Message)
{
	FExToastDescriptor Desc;
	Desc.Message = Message;
	Desc.Duration = 0.0f; // Manual close
	Desc.ProgressConfig.bShowProgressBar = true;
	Desc.ProgressConfig.bAutoCountdown = false;
	return ShowToastFromDescriptor(Desc);
}

void UExUIManagerSubsystem::HandleToastClosed(UExToastWidget* ClosedToast)
{
	ActiveToasts.RemoveAll([ClosedToast](const TWeakObjectPtr<UExToastWidget>& Weak)
	{
		return !Weak.IsValid() || Weak.Get() == ClosedToast;
	});

	// Process Pending Queue
	if (ActiveToasts.Num() < MaxVisibleToasts && PendingToastsQueue.Num() > 0)
	{
		FExToastDescriptor NextDesc = PendingToastsQueue[0];
		PendingToastsQueue.RemoveAt(0);

		ShowToastFromDescriptor(NextDesc);
	}
}
