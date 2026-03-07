// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ExRunnerStatComponent.generated.h"

class UMoverComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunnerSpeedChanged, float, NewSpeed);

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

private:
	/** 소유 Pawn 캐시 (BeginPlay 시 설정) */
	TWeakObjectPtr<APawn> BoundPawn;

	/**
	 * Mover 시스템 컴포넌트 캐시.
	 * APawn::GetVelocity()는 Mover에서 0을 반환하므로, MoverComponent::GetVelocity()를 직접 사용합니다.
	 */
	TWeakObjectPtr<UMoverComponent> CachedMoverComponent;

	/** 타이머 핸들 (컴포넌트 소멸 시 자동 정리) */
	FTimerHandle StatPollTimerHandle;

	/**
	 * StatPollInterval 주기로 호출되는 스탯 수집 함수.
	 * 새로운 스탯 추가 시 이 함수에서 수집 로직을 확장합니다.
	 */
	UFUNCTION()
	void UpdateStats();

	// -- 내부 스탯 저장 변수 --

	/** 현재 달리기 속도 (cm/s). 멀티플레이 확장 시 ReplicatedUsing으로 전환 가능 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Runner|Stats", meta=(AllowPrivateAccess))
	float CurrentRunningSpeed = 0.0f;
};
