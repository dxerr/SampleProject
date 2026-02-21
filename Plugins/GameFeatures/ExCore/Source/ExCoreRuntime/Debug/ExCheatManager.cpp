// Copyright ExFrameWork. All Rights Reserved.

#include "ExCheatManager.h"
#include "ExDebugStateSubsystem.h"
#include "ExGameplayTags.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogExCheat, Log, All);

UExCheatManager::UExCheatManager()
{
}

void UExCheatManager::InitCheatManager()
{
	Super::InitCheatManager();

	UE_LOG(LogExCheat, Log, TEXT("ExCheatManager 초기화 완료. GameFeatureAction_AddCheats를 통해 Extension 등록."));
}

void UExCheatManager::ExDebug(FString Category)
{
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!GI)
	{
		UE_LOG(LogExCheat, Warning, TEXT("ExDebug: GameInstance를 찾을 수 없습니다."));
		return;
	}

	UExDebugStateSubsystem* DebugState = GI->GetSubsystem<UExDebugStateSubsystem>();
	if (!DebugState)
	{
		UE_LOG(LogExCheat, Warning, TEXT("ExDebug: DebugStateSubsystem을 찾을 수 없습니다."));
		return;
	}

	// "Ex.Debug.[Category]" 태그 문자열 생성 및 탐색
	const FString TagString = FString::Printf(TEXT("Ex.Debug.%s"), *Category);
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagString), false);
	
	if (!Tag.IsValid())
	{
		UE_LOG(LogExCheat, Warning, TEXT("ExDebug: 유효하지 않은 카테고리 '%s' (태그: %s)"), *Category, *TagString);
		UE_LOG(LogExCheat, Log, TEXT("사용 가능한 태그: Ex.Debug.Path, Ex.Debug.Chunk, Ex.Debug.Slope 등"));
		return;
	}

	// 토글
	const bool bNewState = !DebugState->IsCheatEnabled(Tag);
	DebugState->SetCheatEnabled(Tag, bNewState);

	UE_LOG(LogExCheat, Log, TEXT("ExDebug: [%s] = %s"), *Category, bNewState ? TEXT("ON") : TEXT("OFF"));
}

void UExCheatManager::ExDebugStatus()
{
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!GI) return;

	UExDebugStateSubsystem* DebugState = GI->GetSubsystem<UExDebugStateSubsystem>();
	if (!DebugState) return;

	DebugState->PrintAllStates();
}

void UExCheatManager::ExDebugReset()
{
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!GI) return;

	UExDebugStateSubsystem* DebugState = GI->GetSubsystem<UExDebugStateSubsystem>();
	if (!DebugState) return;

	DebugState->ResetAllStates();
	UE_LOG(LogExCheat, Log, TEXT("ExDebugReset: 모든 디버그 상태가 초기화되었습니다."));
}
