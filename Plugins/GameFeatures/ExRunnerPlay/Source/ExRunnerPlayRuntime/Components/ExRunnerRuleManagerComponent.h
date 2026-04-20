// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "../Struct/EExRunnerGameOverReason.h"
#include "ExRunnerRuleManagerComponent.generated.h"

class UExRunnerRuleBase;
class UExRunnerRuleConfig;
class AExRunnerGameState;
class UExGameplayEventSubsystem;

/**
 * UExRunnerRuleManagerComponent
 * AExRunnerGameMode에 부착되어 룰 목록을 관리하는 컴포넌트
 *
 * - DataAsset(UExRunnerRuleConfig)에서 룰 목록을 로드
 * - 매치 시작 시 ActivateAllRules()로 모든 룰 활성화
 * - 각 룰의 OnRuleTriggered 델리게이트를 구독하여 게임오버 처리
 * - 복수 룰 동시 발동 방지 (bGameOverHandled 플래그)
 */
UCLASS(ClassGroup=(ExRunner), meta=(BlueprintSpawnableComponent))
class EXRUNNERPLAYRUNTIME_API UExRunnerRuleManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UExRunnerRuleManagerComponent();

	// ── 설정 ──────────────────────────────────────────────────────

	/**
	 * 사용할 룰 조합 프리셋 태그
	 * GameMode BP의 컴포넌트 Details에서 DataCenter 로드용 태그(Ex.Runner.Rule) 설정
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule", meta = (Categories = "Ex.Runner.Rule"))
	FGameplayTag RuleConfigTag;

	// ── 공개 인터페이스 ────────────────────────────────────────────

	/** 매치 시작 시 GameMode가 호출 */
	void ActivateAllRules();

	/** 매치 종료/게임오버 시 GameMode가 호출 */
	void DeactivateAllRules();

	/** Kill Volume 스폰 — FallDeath 룰의 ActivateRule()에서 호출 */
	UShapeComponent* SpawnKillVolume(float KillVolumeZ);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** 룰 발동 시 콜백 — GameState에 이유 설정 후 EndMatch 브로드캐스트 */
	void OnRuleTriggered(FGameplayTag ResultTag);

	/** ResultTag → EExRunnerGameOverReason 변환 */
	EExRunnerGameOverReason TagToReason(const FGameplayTag& ResultTag) const;

	/** 복수 룰 동시 발동 방지 플래그 */
	bool bGameOverHandled = false;

	/** 스폰된 Kill Volume 컴포넌트 (FallDeath 룰 파괴 시 정리) */
	UPROPERTY()
	TObjectPtr<class UBoxComponent> SpawnedKillVolumeComp;

	/** 캐싱된 GameState 참조 */
	UPROPERTY()
	TObjectPtr<AExRunnerGameState> CachedGameState;

	/** DataCenter에서 로드한 RuleConfig 캐싱 */
	UPROPERTY(Transient)
	TObjectPtr<UExRunnerRuleConfig> CachedRuleConfig;

	/** 캐싱된 EventSubsystem 참조 */
	UPROPERTY()
	TObjectPtr<UExGameplayEventSubsystem> CachedEventSubsystem;

	/** 활성화된 룰 인스턴스 목록 */
	UPROPERTY()
	TArray<TObjectPtr<UExRunnerRuleBase>> ActiveRules;
};
