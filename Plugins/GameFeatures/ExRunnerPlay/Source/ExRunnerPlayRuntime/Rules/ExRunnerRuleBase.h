// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GameplayTagContainer.h"
#include "ExRunnerRuleBase.generated.h"

class AExRunnerGameMode;
class UExGameplayEventSubsystem;
struct FGameplayTag;

/** RuleManagerComponent가 룰 발동 시 수신하는 델리게이트 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnRuleTriggered, FGameplayTag /*ResultTag*/);

/**
 * UExRunnerRuleBase
 * 러너 게임 룰의 추상 베이스 클래스 (Strategy Pattern)
 *
 * - Abstract: 직접 인스턴스화 불가, 반드시 서브클래스 구현 필요
 * - EditInlineNew: DataAsset(UExRunnerRuleConfig) 내부에서 인라인 편집 가능
 *
 * ⚠️ 구현 주의: UObject 기반이므로 GetWorld() 직접 호출 불가.
 *    InitializeRule()로 주입받은 GameMode 참조를 통해
 *    InGameMode->GetWorld()->GetSubsystem<UExGameplayEventSubsystem>() 경로로 접근할 것.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class EXRUNNERPLAYRUNTIME_API UExRunnerRuleBase : public UObject
{
	GENERATED_BODY()

public:
	// ── 수명 주기 ──────────────────────────────────────────────────

	/** GameMode 참조 주입 — 반드시 ActivateRule() 전에 호출 */
	virtual void InitializeRule(AExRunnerGameMode* InGameMode);

	/** 매치 시작 시 RuleManagerComponent가 호출 */
	virtual void ActivateRule();

	/** 게임오버/매치 종료 시 RuleManagerComponent가 호출 */
	virtual void DeactivateRule();

	/**
	 * 프레임마다 호출되는 업데이트 — bTickEnabled=true인 룰만 호출됨
	 * RuleManagerComponent의 TickComponent에서 직접 호출
	 */
	virtual void TickRule(float DeltaTime);

	// ── 에디터 노출 설정 ─────────────────────────────────────────

	/** 룰 발동 시 브로드캐스트할 GameplayTag (FallDeath, TimeUp, GoalReached 등) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule")
	FGameplayTag TriggerTag;

	/** true면 RuleManagerComponent가 매 프레임 TickRule()을 호출 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule")
	bool bTickEnabled = false;

	// ── 발동 알림 (RuleManagerComponent가 구독) ──────────────────

	/** 룰 조건 충족 시 Broadcast(TriggerTag) 호출 */
	FOnRuleTriggered OnRuleTriggered;

protected:
	/** InitializeRule()로 주입된 GameMode 참조 */
	UPROPERTY()
	TObjectPtr<AExRunnerGameMode> CachedGameMode = nullptr;

	/** GameMode->GetWorld()에서 캐싱된 이벤트 서브시스템 */
	UPROPERTY()
	TObjectPtr<UExGameplayEventSubsystem> CachedEventSubsystem = nullptr;
};
