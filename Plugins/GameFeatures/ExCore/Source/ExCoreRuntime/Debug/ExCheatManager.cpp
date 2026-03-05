// Copyright ExFrameWork. All Rights Reserved.

#include "ExCheatManager.h"
#include "ExDebugStateSubsystem.h"
#include "ExGameplayTags.h"
#include "GameModes/ExGameModeBase.h"
#include "GameModes/ExGameStateBase.h"
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

void UExCheatManager::ExSetMatchPhase(FString PhaseTagString)
{
	UWorld* World = GetWorld();
	if (!World) return;

	// 권한 체크: 호스트/서버에서만 유효
	if (World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogExCheat, Warning, TEXT("ExSetMatchPhase: 클라이언트에서는 매치 상태를 변경할 수 없습니다. 서버/호스트 권한이 필요합니다."));
		return;
	}

	AExGameStateBase* GameState = World->GetGameState<AExGameStateBase>();
	if (!GameState)
	{
		UE_LOG(LogExCheat, Error, TEXT("ExSetMatchPhase: 활성화된 AExGameStateBase를 찾을 수 없습니다. (GameStateBase 클래스가 설정되었는지 확인하세요)"));
		return;
	}

	FGameplayTag NewPhaseTag = FGameplayTag::RequestGameplayTag(FName(*PhaseTagString), false);
	if (!NewPhaseTag.IsValid())
	{
		UE_LOG(LogExCheat, Warning, TEXT("ExSetMatchPhase: 유효하지 않은 태그입니다. [%s]"), *PhaseTagString);
		return;
	}

	UE_LOG(LogExCheat, Log, TEXT("[Cheat] 강제로 매치 페이즈를 변경합니다: %s"), *NewPhaseTag.ToString());
	
	// GameMode 대신 GameState에 추가될 퍼블릭 함수를 직접 호출 (치트 옵션 true)
	GameState->SetMatchPhase(NewPhaseTag, true);
}
