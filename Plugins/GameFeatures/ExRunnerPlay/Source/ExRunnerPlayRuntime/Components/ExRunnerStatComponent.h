// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ExRunnerStatComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunnerSpeedChanged, float, NewSpeed);

/**
 * [아키텍처 2.2 도메인별 데이터 중앙 집중화 체계]
 * 러너 게임 특화 상태 데이터(달리기 속도, 획득 코인 등)를 중앙에서 보관하고 관리하는 컴포넌트입니다.
 * 순수 데이터 저장소의 역할을 하며 이벤트(Broadcast)를 통해서만 UI에 변경을 알립니다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class EXRUNNERPLAYRUNTIME_API UExRunnerStatComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UExRunnerStatComponent();

	/** 현재 속도 반환 */
	UFUNCTION(BlueprintPure, Category = "Runner|Stats")
	float GetCurrentRunningSpeed() const { return CurrentRunningSpeed; }

	/** 속도를 갱신하고 변경폭이 의미있을 때만 델리게이트 발송 */
	UFUNCTION(BlueprintCallable, Category = "Runner|Stats")
	void SetCurrentRunningSpeed(float NewSpeed);

	/** UI 바인딩용 스피드 변경 이벤트 */
	UPROPERTY(BlueprintAssignable, Category = "Runner|Stats|Events")
	FOnRunnerSpeedChanged OnRunnerSpeedChanged;

protected:
	/**
	 * 멀티플레이어 환경 확장 고려 시 ReplicatedUsing으로 OnRep 콜백을 이어주면 됩니다.
	 * 현재는 로컬 전용으로 동작하는 구조입니다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Runner|Stats")
	float CurrentRunningSpeed = 0.0f;
};
