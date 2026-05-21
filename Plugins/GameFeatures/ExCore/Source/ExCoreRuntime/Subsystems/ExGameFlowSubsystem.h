// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "ExGameFlowSubsystem.generated.h"

class UExExperienceDefinition;

// Travel 요청 델리게이트 (서버 권한인 GameMode가 이를 구독하여 실제 Travel 수행)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRequestTravel, const FString&, MapURL);

// Flow 변경 알림 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFlowStateChanged, const FGameplayTag&, OldState, const FGameplayTag&, NewState);

/**
 * 전역적인 앱 레벨 멀티플레이어 Flow 상태(Boot -> IDP -> Lobby -> InGame)를 관리하는 서브시스템.
 * 주의: GameInstanceSubsystem은 복제(Replication)를 지원하지 않으므로, 
 * 서버가 주도하는 전역 변경 사항은 플레이어 컨트롤러의 RPC 또는 GameState의 OnRep 델리게이트를 통해 연동시켜야 합니다.
 */
UCLASS()
class EXCORERUNTIME_API UExGameFlowSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Subsystem Init/Deinit
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 현재 Flow 상태를 변경합니다. 허용된 전이인지 내부적으로 검증합니다. */
	UFUNCTION(BlueprintCallable, Category = "ExFlow")
	void SetFlowState(FGameplayTag NewState);

	/** 현재 Flow 상태를 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ExFlow")
	FGameplayTag GetCurrentFlowState() const { return CurrentFlowState; }

	/** 특정 맵으로 서버 이동(Listen/Dedicated) 또는 클라이언트 세션 접속을 간접 요청합니다. */
	UFUNCTION(BlueprintCallable, Category = "ExFlow")
	void RequestTravel(const FString& MapURL);

	/** 
	 * [UI 호출용] 경험 데이터 에셋(Experience Definition) 기반으로 인게임 전환을 요청합니다.
	 * 맵 이름 대신 데이터 에셋을 인자로 받아, 그 안에서 지정된 맵 경로를 찾아 트래블합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ExFlow", meta=(DisplayName="Transition To Experience"))
	void TransitionToExperience(const UExExperienceDefinition* ExperienceConfig);

	UFUNCTION()
	void OnMatchConnectionFailed(const FString& ErrorMessage);

public:
	UPROPERTY(BlueprintAssignable, Category = "ExFlow")
	FOnFlowStateChanged OnFlowStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "ExFlow")
	FOnRequestTravel OnRequestTravel;

protected:
	/** 가능한 플로우 전이 경로 정의 */
	TMap<FGameplayTag, TArray<FGameplayTag>> AllowedTransitions;

private:
	FGameplayTag CurrentFlowState;
	FString PendingErrorMessage;
	FDelegateHandle PostLoadMapHandle;
};
