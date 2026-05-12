// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Struct/FExBuffDefinition.h"
#include "ExRunnerBuffComponent.generated.h"

class UExRunnerInputComponent;
class UExRunnerMovementComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBuffActivated,   EExBuffType, BuffType, float, Duration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam (FOnBuffDeactivated,  EExBuffType, BuffType);
/** 폴링 주기마다 발행 — UI ProgressBar 카운트다운 용 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBuffTimeUpdated,  EExBuffType, BuffType, float, RemainingTime);

/**
 * UExRunnerBuffComponent
 *
 * 러너 게임의 버프/디버프 상태를 중앙에서 관리하는 컴포넌트.
 * - ExGameplayEventSubsystem 구독으로 버프 이벤트 수신
 * - RemoveList 기반 우선순위 충돌 처리
 * - Duration 기반 타이머 + 연장 방식
 * - Weight 기반 이동 속도 배율 적용 (SpeedUp 전용)
 * - 서버 권한에서만 버프 로직 실행 / ActiveBuffs 복제로 클라이언트 UI 동기화
 */
UCLASS(Blueprintable, ClassGroup=(ExRunner), meta=(BlueprintSpawnableComponent))
class EXRUNNERPLAYRUNTIME_API UExRunnerBuffComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UExRunnerBuffComponent();

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	// ========== 외부 API ==========

	/**
	 * 버프를 활성화합니다.
	 * - RemoveList에 있는 활성 버프를 먼저 종료
	 * - 동일 타입이 이미 있으면 Duration 연장, 없으면 신규 등록
	 * - 서버 권한에서만 실행됩니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ExBuff")
	void ActivateBuff(FExBuffDefinition Def);

	/**
	 * 특정 타입의 버프를 강제 종료합니다. (이벤트 기반 해제에 사용)
	 */
	UFUNCTION(BlueprintCallable, Category = "ExBuff")
	void TerminateBuff(EExBuffType BuffType);

	/**
	 * 모든 활성 버프를 해제합니다. (낙사, 라운드 종료 등)
	 */
	UFUNCTION(BlueprintCallable, Category = "ExBuff")
	void ClearAllBuffs();

	/** 특정 버프가 현재 활성화되어 있는지 확인 */
	UFUNCTION(BlueprintPure, Category = "ExBuff")
	bool IsBuffActive(EExBuffType BuffType) const;

	/** 특정 버프의 잔여 시간 반환 (비활성이면 0) */
	UFUNCTION(BlueprintPure, Category = "ExBuff")
	float GetBuffRemainingTime(EExBuffType BuffType) const;

	// ========== UI 연동 델리게이트 ==========

	/** 버프가 새로 활성화되거나 갱신될 때 발행 */
	UPROPERTY(BlueprintAssignable, Category = "ExBuff|Events")
	FOnBuffActivated OnBuffActivated;

	/** 버프가 종료될 때 발행 */
	UPROPERTY(BlueprintAssignable, Category = "ExBuff|Events")
	FOnBuffDeactivated OnBuffDeactivated;

	/** 폴링 주기마다 발행 — UI ProgressBar 카운트다운에 사용 */
	UPROPERTY(BlueprintAssignable, Category = "ExBuff|Events")
	FOnBuffTimeUpdated OnBuffTimeUpdated;

	/** 타이머 폴링 주기 (초). StatComponent와 동일하게 맞추는 것을 권장 */
	UPROPERTY(EditAnywhere, Category = "ExBuff|Config", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float PollInterval = 0.1f;

private:
	// ========== 내부 구현 ==========

	/** 폴링 타이머 핸들 */
	FTimerHandle PollTimerHandle;

	/** 현재 활성화된 버프 목록 (서버 → 클라이언트 복제) */
	UPROPERTY(ReplicatedUsing = OnRep_ActiveBuffs)
	TArray<FExActiveBuffState> ActiveBuffs;

	UFUNCTION()
	void OnRep_ActiveBuffs();

	/** 매 PollInterval 마다 잔여 시간을 감소시키고 만료 버프를 종료 */
	UFUNCTION()
	void UpdateBuffTimers();

	/** 버프 효과를 실제로 적용 */
	void ApplyBuffEffect(const FExActiveBuffState& Buff);

	/** 버프 효과를 실제로 복구 */
	void RevertBuffEffect(EExBuffType BuffType);

	// ========== 이벤트 구독 콜백 ==========

	UFUNCTION()
	void OnSpeedUpBuffEvent(FGameplayTag Tag, const struct FExGameplayEventPayload& Payload);

	UFUNCTION()
	void OnSpeedDownBuffEvent(FGameplayTag Tag, const struct FExGameplayEventPayload& Payload);

	UFUNCTION()
	void OnClearAllBuffsEvent(FGameplayTag Tag, const struct FExGameplayEventPayload& Payload);

	/**
	 * Match_Playing 단계 시작 시 기본 Sprint를 활성화합니다.
	 * ExGameplayEventSubsystem의 Match.Playing 이벤트를 수신합니다.
	 */
	UFUNCTION()
	void OnMatchPlayingStarted(FGameplayTag Tag, const struct FExGameplayEventPayload& Payload);

	/**
	 * GameplayTag → EExBuffType 변환 헬퍼.
	 * 데이터 에셋 RemoveList(FGameplayTag 배열)를 EExBuffType으로 변환할 때 사용.
	 * 지원하지 않는 태그는 TOptional 보유(nullopt)으로 스킵.
	 */
	static TOptional<EExBuffType> TagToBuffType(const FGameplayTag& Tag);

	// ========== 컴포넌트 캐시 ==========

	TWeakObjectPtr<APawn> BoundPawn;
	TWeakObjectPtr<UExRunnerInputComponent>    CachedInputComp;
	TWeakObjectPtr<UExRunnerMovementComponent> CachedMovementComp;
};
