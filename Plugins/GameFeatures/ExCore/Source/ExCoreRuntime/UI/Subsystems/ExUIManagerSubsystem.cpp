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
