// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExItemEffect.h"
#include "ExItemEffect_Score.generated.h"

/**
 * 점수 이펙트 — 획득 시 플레이어에게 점수를 추가한다.
 * PlayerState::AddScore()를 호출하고,
 * ExGameplayEventSubsystem에 Item_PickedUp_Score 이벤트를 브로드캐스트한다.
 *
 * Stateless: ScoreAmount는 에디터 설정값(런타임 불변)
 */
UCLASS(BlueprintType, DisplayName = "Score Effect")
class EXCORERUNTIME_API UExItemEffect_Score : public UExItemEffect
{
	GENERATED_BODY()

public:
	/** 획득 시 추가할 점수 (설정값, 런타임 불변) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect|Score", meta = (ClampMin = "0"))
	float ScoreAmount = 1.f;

	virtual void Execute_Implementation(AActor* Instigator, const UExItemDefinition* ItemDefinition, AExItemPickupBase* ItemActor) override;
	virtual FText GetEffectDescription_Implementation() const override;
};
