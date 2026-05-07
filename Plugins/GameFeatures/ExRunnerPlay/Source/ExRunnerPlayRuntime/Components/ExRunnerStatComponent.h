// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ExRunnerStatComponent.generated.h"

class UMoverComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunnerSpeedChanged, float, NewSpeed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCoinCountChanged, int32, NewCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSprintTimeChanged, float, RemainingTime);

/**
 * [아키텍처 2.2 도메인별 데이터 중앙 집중화 체계]
 * 러너 게임 특화 상태 데이터(달리기 속도, 이동 거리, 코인 등)를 중앙에서 보관하고 관리하는 컴포넌트입니다.
 * 순수 데이터 저장소 역할을 하며, 자체 타이머 주기로 Owner Pawn의 스탯을 수집합니다.
 * 이벤트(Broadcast)를 통해서만 UI에 변경을 알립니다.
 *
 * 확장 가이드:
 * - 새 스탯 추가 시: UPROPERTY(FieldNotify)변수 + 델리게이트 + UpdateStats()에 수집 로직 추가
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class EXRUNNERPLAYRUNTIME_API UExRunnerStatComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UExRunnerStatComponent();

protected:
	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	// -- 스탯 폴링 설정 --

	/** 스탯 수집 주기 (초). 값이 작을수록 정밀하지만 CPU 부하 상승. 기본 0.1초 = 10fps 갱신 */
	UPROPERTY(EditAnywhere, Category = "Runner|Stats|Config", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float StatPollInterval = 0.1f;

	// -- 현재 스탯 Getter --

	/** 현재 속도 반환 */
	UFUNCTION(BlueprintPure, Category = "Runner|Stats")
	float GetCurrentRunningSpeed() const { return CurrentRunningSpeed; }

	// -- UI 바인딩용 이벤트 --

	/** 속도가 의미 있는 변화 폭으로 변경될 때 발행되는 이벤트 */
	UPROPERTY(BlueprintAssignable, Category = "Runner|Stats|Events")
	FOnRunnerSpeedChanged OnRunnerSpeedChanged;

	// -- 외부에서 값을 직접 설정해야 할 경우 (예: 서버 권위 보정 등) --

	/** 속도를 외부에서 직접 설정합니다. 의미 있는 변화에만 Broadcast 발행 */
	UFUNCTION(BlueprintCallable, Category = "Runner|Stats")
	void SetCurrentRunningSpeed(float NewSpeed);

	// -- 이동 거리 (Distance) --
	
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunnerDistanceChanged, float, NewDistance);

	/** 거리가 의미 있는 변화 폭으로 변경될 때 발행되는 이벤트 */
	UPROPERTY(BlueprintAssignable, Category = "Runner|Stats|Events")
	FOnRunnerDistanceChanged OnRunnerDistanceChanged;

	/** 현재 누적 이동 거리 반환 */
	UFUNCTION(BlueprintPure, Category = "Runner|Stats")
	float GetCurrentDistance() const { return CurrentDistance; }

	/** 이동 거리를 갱신합니다. */
	UFUNCTION(BlueprintCallable, Category = "Runner|Stats")
	void SetCurrentDistance(float NewDistance);

	// -- 코인 시스템 --

	/** 코인 개수가 변경될 때 발행되는 이벤트 */
	UPROPERTY(BlueprintAssignable, Category = "Runner|Stats|Events")
	FOnCoinCountChanged OnCoinCountChanged;

	/** 현재 코인 개수 반환 */
	UFUNCTION(BlueprintPure, Category = "Runner|Stats")
	int32 GetCoinCount() const { return CoinCount; }

	/** 코인 개수를 강제로 조절합니다. (+/-) */
	UFUNCTION(BlueprintCallable, Category = "Runner|Stats")
	void AddCoinCount(int32 Amount);

	// -- 스프린트(달리기) 시스템 --

	/** 스프린트 남은 시간이 변경될 때 발행되는 이벤트 */
	UPROPERTY(BlueprintAssignable, Category = "Runner|Stats|Events")
	FOnSprintTimeChanged OnSprintTimeChanged;

	/** 현재 남은 스프린트 가능 시간 (초) 반환 */
	UFUNCTION(BlueprintPure, Category = "Runner|Stats")
	float GetSprintRemainingTime() const { return SprintRemainingTime; }

	/** 특정 시간(초)만큼 스프린트 모드를 활성화/연장합니다. */
	UFUNCTION(BlueprintCallable, Category = "Runner|Stats")
	void ActivateSprint(float Duration);

private:
	/** 소유 Pawn 캐시 (BeginPlay 시 설정) */
	TWeakObjectPtr<APawn> BoundPawn;

	/** Mover 시스템 캐시 (속도 폴링용) */
	TWeakObjectPtr<UMoverComponent> CachedMoverComponent;

	/** Runner InputComponent 캐시 (스프린트 상태 제어용) */
	TWeakObjectPtr<class UExRunnerInputComponent> CachedInputComponent;

	/** 타이머 핸들 (컴포넌트 소멸 시 자동 정리) */
	FTimerHandle StatPollTimerHandle;

	/** StatPollInterval 주기로 호출되는 스탯 수집 함수 */
	UFUNCTION()
	void UpdateStats();

	/** 스탯 수집 루프 중 스프린트 시간 차감을 담당 */
	void UpdateSprintTimer();

	// -- ExGameplayEventSubsystem 구독 이벤트 콜백 --

	/** 점수(코인 등) 아이템 획득 이벤트 수신 */
	UFUNCTION()
	void OnScorePickedUp(FGameplayTag Tag, const struct FExGameplayEventPayload& Payload);

	/** 속도 증가 버프 획득 이벤트 수신 */
	UFUNCTION()
	void OnSpeedUpBuff(FGameplayTag Tag, const struct FExGameplayEventPayload& Payload);

	// -- 내부 상태 데이터 --

	/** 현재 달리기 속도 (cm/s). 멀티플레이 확장 시 ReplicatedUsing으로 전환 가능 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Runner|Stats", meta=(AllowPrivateAccess))
	float CurrentRunningSpeed = 0.0f;

	/** 현재 달린 거리 (cm) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Runner|Stats", meta=(AllowPrivateAccess))
	float CurrentDistance = 0.0f;

	/** 획득한 총 코인 점수 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Runner|Stats", meta=(AllowPrivateAccess))
	int32 CoinCount = 0;

	/** 현재 남은 스프린트 활성화 시간 (초) */
	UPROPERTY(ReplicatedUsing = OnRep_SprintRemainingTime, VisibleAnywhere, BlueprintReadOnly, Category = "Runner|Stats", meta=(AllowPrivateAccess="true"))
	float SprintRemainingTime = 0.0f;

	UFUNCTION()
	void OnRep_SprintRemainingTime();

	/** 스프린트 버프 활성화 여부 (서버 -> 클라이언트 동기화) */
	UPROPERTY(ReplicatedUsing = OnRep_IsSprintBuffActive, VisibleAnywhere, BlueprintReadOnly, Category = "Runner|Stats", meta=(AllowPrivateAccess="true"))
	bool bIsSprintBuffActive = false;

	UFUNCTION()
	void OnRep_IsSprintBuffActive();
};
