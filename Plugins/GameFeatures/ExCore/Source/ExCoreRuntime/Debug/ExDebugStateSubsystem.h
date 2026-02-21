// Copyright ExFrameWork. All Rights Reserved.
// GameInstance 레벨 전역 디버그 상태 관리 서브시스템

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "ExDebugTypes.h"
#include "ExDebugStateSubsystem.generated.h"

/**
 * UExDebugStateSubsystem
 * GameInstance 수명주기를 가진 전역 디버그 상태 관리 서브시스템
 * 
 * 핵심 기능:
 * - GameplayTag 기반 치트 상태(On/Off, 수치) 관리
 * - 레벨 전환 시에도 상태 유지 (GameInstanceSubsystem)
 * - 상태 변경 시 ExGameplayEventSubsystem으로 브로드캐스트
 * - Feature 모듈에서 Tag만으로 상태를 조회/변경 가능
 * 
 * 사용 예:
 *   auto* DS = GetGameInstance()->GetSubsystem<UExDebugStateSubsystem>();
 *   DS->SetCheatEnabled(TAG_Ex_Debug_Path, true);
 *   if (DS->IsCheatEnabled(TAG_Ex_Debug_Path)) { ... }
 */
UCLASS()
class EXCORERUNTIME_API UExDebugStateSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ========== 수명주기 ==========
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// ========== 치트 상태 관리 (Toggle) ==========

	/**
	 * 치트 활성화/비활성화
	 * 상태 변경 시 ExGameplayEventSubsystem을 통해 브로드캐스트
	 * @param CheatTag 치트를 식별하는 GameplayTag
	 * @param bEnabled 활성화 여부
	 */
	UFUNCTION(BlueprintCallable, Category = "Ex|Debug")
	void SetCheatEnabled(FGameplayTag CheatTag, bool bEnabled);

	/** 치트 활성화 여부 조회 */
	UFUNCTION(BlueprintPure, Category = "Ex|Debug")
	bool IsCheatEnabled(FGameplayTag CheatTag) const;

	/** 치트 토글 (현재 상태 반전) */
	UFUNCTION(BlueprintCallable, Category = "Ex|Debug")
	bool ToggleCheat(FGameplayTag CheatTag);

	// ========== 치트 수치 관리 (Slider) ==========

	/**
	 * 치트 수치 설정
	 * @param CheatTag 치트를 식별하는 GameplayTag
	 * @param Value 설정할 수치
	 */
	UFUNCTION(BlueprintCallable, Category = "Ex|Debug")
	void SetCheatValue(FGameplayTag CheatTag, float Value);

	/** 치트 수치 조회 */
	UFUNCTION(BlueprintPure, Category = "Ex|Debug")
	float GetCheatValue(FGameplayTag CheatTag) const;

	// ========== 전체 상태 관리 ==========

	/** 모든 치트 상태 초기화 */
	UFUNCTION(BlueprintCallable, Category = "Ex|Debug")
	void ResetAllStates();

	/** 활성화된 모든 치트 로그 출력 */
	void PrintAllStates() const;

	/** 전체 상태 맵 참조 (읽기 전용) */
	const TMap<FGameplayTag, FExDebugCheatState>& GetAllCheatStates() const { return CheatStates; }

protected:
	/**
	 * 상태 변경 시 ExGameplayEventSubsystem으로 브로드캐스트
	 * @param CheatTag 변경된 치트 태그
	 */
	void BroadcastStateChange(FGameplayTag CheatTag);

private:
	/** GameplayTag별 치트 상태 맵 (레벨 간 유지) */
	UPROPERTY()
	TMap<FGameplayTag, FExDebugCheatState> CheatStates;
};
