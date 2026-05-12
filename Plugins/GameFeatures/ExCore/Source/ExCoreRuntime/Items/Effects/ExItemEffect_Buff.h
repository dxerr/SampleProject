// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ExItemEffect.h"
#include "ExItemEffect_Buff.generated.h"

/**
 * 범용 버프 이펙트 — 획득 시 BuffTag + Magnitude + Duration을
 * ExGameplayEventSubsystem으로 브로드캐스트한다.
 * 관심 있는 시스템(MovementComponent, StatComponent 등)이 각자 구독하여 처리한다.
 * Core가 Feature를 몰라도 되는 이벤트 디커플링 달성.
 *
 * Stateless: BuffTag/Magnitude/Duration은 설정값(런타임 불변).
 *            실제 타이머/상태 관리는 수신 컴포넌트가 담당.
 */
UCLASS(BlueprintType, DisplayName = "Buff Effect")
class EXCORERUNTIME_API UExItemEffect_Buff : public UExItemEffect
{
	GENERATED_BODY()

public:
	/** 버프를 식별하는 GameplayTag (Ex.Buff.SpeedUp, Ex.Buff.Invincible 등) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect|Buff")
	FGameplayTag BuffTag;

	/** 버프 수치 (설정값, 의미는 수신 측에서 해석: 속도 배율, 방어력 증가량 등) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect|Buff")
	float Magnitude = 1.5f;

	/** 버프 지속 시간 (설정값, 초). 0이면 즉시 적용(영구) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect|Buff", meta = (ClampMin = "0"))
	float Duration = 5.0f;

	/**
	 * 이 버프가 활성화될 때 먼저 제거(해제)할 버프 태그 목록.
	 * 예) SpeedUp의 RemoveList에 Ex.Buff.SpeedDown을 넣으면
	 *     SpeedDown 중 SpeedUp을 먹으면 SpeedDown이 먼저 해제됩니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect|Buff")
	TArray<FGameplayTag> RemoveList;

	virtual void Execute_Implementation(AActor* Instigator, const UExItemDefinition* ItemDefinition, AExItemPickupBase* ItemActor) override;
	virtual FText GetEffectDescription_Implementation() const override;
};
