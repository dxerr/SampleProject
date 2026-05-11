// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "GameplayTagContainer.h"
#include "ExGameStateBase.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMatchPhaseChanged, const FGameplayTag&, OldPhase, const FGameplayTag&, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCountdownChanged, int32, NewCountdown);

/**
 * ExCore의 기본 GameState 클래스입니다.
 * 게임 매치의 단계(Phase)를 관리하고 클라이언트로 복제(Replicate)하여 UI 갱신을 주도합니다.
 */
UCLASS()
class EXCORERUNTIME_API AExGameStateBase : public AGameStateBase
{
	GENERATED_BODY()
	
public:
	AExGameStateBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	/**
	 * 현재 매치 페이즈를 가져옵니다.
	 */
	UFUNCTION(BlueprintPure, Category = "ExMatch")
	FGameplayTag GetCurrentMatchPhase() const { return CurrentMatchPhase; }

	/**
	 * 로컬 클라이언트에서 매치 페이즈 변경 시 발송되는 델리게이트입니다.
	 * 서버에서 CurrentMatchPhase가 변경되면 OnRep를 통해 클라이언트에서 Broadcast 됩니다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "ExMatch")
	FOnMatchPhaseChanged OnMatchPhaseChanged;

	/**
	 * 서버에서 매치 페이즈를 설정하고 클라이언트로 전파
	 * @param NewPhase 변경할 상태 태그
	 * @param bForceTransition 전이 규칙을 무시하고 강제로 변경 (치트용)
	 */
	UFUNCTION(BlueprintCallable, Category = "ExMatch")
	void SetMatchPhase(FGameplayTag NewPhase, bool bForceTransition = false);

	/**
	 * 매치가 현재 활성화 상태인지(Playing 이상) 확인합니다.
	 */
	UFUNCTION(BlueprintPure, Category = "ExMatch")
	bool IsMatchActive() const;

	/**
	 * 카운트다운 남은 초를 가져옵니다.
	 */
	UFUNCTION(BlueprintPure, Category = "ExMatch")
	int32 GetCountdownSeconds() const { return CountdownSecondsRemaining; }

	/** 로컬 클라이언트에서 카운트다운이 변경될 때 호출되는 델리게이트 */
	UPROPERTY(BlueprintAssignable, Category = "ExMatch")
	FOnCountdownChanged OnCountdownChanged;

	/**
	 * 서버에서 카운트다운 초를 설정하고 클라이언트로 전파
	 */
	UFUNCTION(BlueprintCallable, Category = "ExMatch")
	void SetCountdownSeconds(int32 NewSeconds);

protected:
	/**
	 * 서버에서 설정되어 모든 클라이언트로 복제되는 남은 카운트다운 초
	 */
	UPROPERTY(ReplicatedUsing = OnRep_CountdownSeconds, BlueprintReadOnly, Category = "ExMatch")
	int32 CountdownSecondsRemaining;

	/** 클라이언트에서 카운트다운 변수 복제 시 호출되는 콜백. */
	UFUNCTION()
	virtual void OnRep_CountdownSeconds(int32 OldSeconds);
	/**
	 * 서버에서 설정되어 모든 클라이언트로 복제되는 현재 매치 페이즈 태그.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_MatchPhase, BlueprintReadOnly, Category = "ExMatch")
	FGameplayTag CurrentMatchPhase;

	/** 클라이언트에서 변수 복제 시 호출되는 콜백. */
	UFUNCTION()
	virtual void OnRep_MatchPhase(const FGameplayTag& OldPhase);

	/**
	 * 매치 페이즈가 변경되었을 때 하위 클래스에서 추가 로직을 구현하기 위한 가상 함수.
	 */
	virtual void HandleMatchPhaseChanged(const FGameplayTag& OldPhase, const FGameplayTag& NewPhase);

	friend class AExGameModeBase; // GameModeBase에서만 페이즈를 변경하도록 friend 선언
};
