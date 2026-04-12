// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExRunnerRuleBase.h"
#include "GameplayTagContainer.h"
#include "ExRunnerRule_FallDeath.generated.h"

class UShapeComponent;

/**
 * UExRunnerRule_FallDeath
 * 플레이어 낙하 사망 감지 룰
 *
 * GameMode를 통해 Kill Volume(UBoxComponent 트리거)을 월드에 배치하고,
 * 플레이어가 진입 시 EventSubsystem 태그로 알림 → 폴링 없는 이벤트 구독 방식
 */
UCLASS(BlueprintType, EditInlineNew)
class EXRUNNERPLAYRUNTIME_API UExRunnerRule_FallDeath : public UExRunnerRuleBase
{
	GENERATED_BODY()

public:
	UExRunnerRule_FallDeath();

	/** Kill Volume 배치 Z 좌표 (cm, 바닥보다 아래) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule|FallDeath")
	float KillVolumeZ = -1500.f;

	/** Kill Volume Overlap 감지 후 브로드캐스트되는 태그 (= Ex.Runner.Player.DeathVolume) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule|FallDeath")
	FGameplayTag DeathVolumeTag;

	virtual void ActivateRule() override;
	virtual void DeactivateRule() override;

private:
	UFUNCTION()
	void OnKillVolumeOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	/** 스폰된 Kill Volume 컴포넌트 (참조만, 소유는 RuleManagerComponent) */
	UPROPERTY()
	TObjectPtr<UShapeComponent> CachedKillVolume;
};
