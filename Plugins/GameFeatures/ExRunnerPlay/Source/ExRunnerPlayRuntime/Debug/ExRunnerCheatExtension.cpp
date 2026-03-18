// Copyright ExFrameWork. All Rights Reserved.

#include "ExRunnerCheatExtension.h"
#include "ExDebugStateSubsystem.h"
#include "ExGameplayTags.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogExRunnerCheats, Log, All);

// ========== 유틸리티 (내부) ==========

namespace ExRunnerCheatUtil
{
	/** DebugStateSubsystem에 안전 접근 */
	static UExDebugStateSubsystem* GetDebugState(const UObject* WorldContextObject)
	{
		if (!WorldContextObject) return nullptr;
		const UWorld* World = WorldContextObject->GetWorld();
		if (!World) return nullptr;
		UGameInstance* GI = World->GetGameInstance();
		if (!GI) return nullptr;
		return GI->GetSubsystem<UExDebugStateSubsystem>();
	}
}

// ========== 경로/청크 시각화 ==========

void UExRunnerCheatExtension::ExRunnerShowPath()
{
	UExDebugStateSubsystem* DS = ExRunnerCheatUtil::GetDebugState(this);
	if (!DS) return;

	const bool bNew = DS->ToggleCheat(TAG_Ex_Debug_Path);
	UE_LOG(LogExRunnerCheats, Log, TEXT("ExRunnerShowPath: 경로 시각화 = %s"), 
		bNew ? TEXT("ON") : TEXT("OFF"));
}

void UExRunnerCheatExtension::ExRunnerShowChunk()
{
	UExDebugStateSubsystem* DS = ExRunnerCheatUtil::GetDebugState(this);
	if (!DS) return;

	const bool bNew = DS->ToggleCheat(TAG_Ex_Debug_Chunk);
	UE_LOG(LogExRunnerCheats, Log, TEXT("ExRunnerShowChunk: 청크 시각화 = %s"), 
		bNew ? TEXT("ON") : TEXT("OFF"));
}

// ========== 꽈배기(경사) 디버그 ==========

void UExRunnerCheatExtension::ExRunnerForceSlope()
{
	UExDebugStateSubsystem* DS = ExRunnerCheatUtil::GetDebugState(this);
	if (!DS) return;

	const bool bNew = DS->ToggleCheat(TAG_Ex_Debug_Slope);
	UE_LOG(LogExRunnerCheats, Log, TEXT("ExRunnerForceSlope: 꽈배기 강제 = %s"), 
		bNew ? TEXT("ON") : TEXT("OFF"));
}

void UExRunnerCheatExtension::ExRunnerSetSlopeTrigger(int32 Count)
{
	UExDebugStateSubsystem* DS = ExRunnerCheatUtil::GetDebugState(this);
	if (!DS) return;

	DS->SetCheatValue(TAG_Ex_Debug_Slope, static_cast<float>(Count));
	UE_LOG(LogExRunnerCheats, Log, TEXT("ExRunnerSetSlopeTrigger: SlopeTriggerCount = %d %s"),
		Count, Count == 0 ? TEXT("(원본 값 사용)") : TEXT("(오버라이드)"));
}

void UExRunnerCheatExtension::ExRunnerShowHeightOffset()
{
	UExDebugStateSubsystem* DS = ExRunnerCheatUtil::GetDebugState(this);
	if (!DS) return;

	// Slope 디버그가 켜져 있어야 HeightOffset 로그도 의미가 있음
	if (!DS->IsCheatEnabled(TAG_Ex_Debug_Slope))
	{
		DS->SetCheatEnabled(TAG_Ex_Debug_Slope, true);
		UE_LOG(LogExRunnerCheats, Log, TEXT("ExRunnerShowHeightOffset: Slope 디버그도 함께 활성화"));
	}

	UE_LOG(LogExRunnerCheats, Log, TEXT("ExRunnerShowHeightOffset: "
		"PathManager에서 TAG_Ex_Debug_Slope를 구독하여 HeightOffset 출력을 구현하세요."));
}

// ========== 속도 디버그 ==========

void UExRunnerCheatExtension::ExRunnerShowSpeed()
{
	UExDebugStateSubsystem* DS = ExRunnerCheatUtil::GetDebugState(this);
	if (!DS) return;

	const bool bNew = DS->ToggleCheat(TAG_Ex_Debug_Speed);
	UE_LOG(LogExRunnerCheats, Log, TEXT("ExRunnerShowSpeed: 속도 디버그 = %s"), 
		bNew ? TEXT("ON") : TEXT("OFF"));
}

// ========== 이동/조향 디버그 ==========

void UExRunnerCheatExtension::ExRunnerShowMovement()
{
	UExDebugStateSubsystem* DS = ExRunnerCheatUtil::GetDebugState(this);
	if (!DS) return;

	const bool bNew = DS->ToggleCheat(TAG_Ex_Debug_Movement);
	UE_LOG(LogExRunnerCheats, Log, TEXT("ExRunnerShowMovement: 이동/조향 디버그 = %s"), 
		bNew ? TEXT("ON") : TEXT("OFF"));
}
