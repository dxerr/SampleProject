// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "ExPlayerStateBase.generated.h"

// 언리얼 APlayerState에 내장된 Score가 변경될 때 Broadcast할 로컬 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnScoreChanged, float, OldScore, float, NewScore);

/**
 * ExCore의 기본 PlayerState 클래스입니다.
 * 플레이어 고유 데이터(Score 등)를 복제하고 클라이언트 UI 갱신을 주도합니다.
 */
UCLASS()
class EXCORERUNTIME_API AExPlayerStateBase : public APlayerState
{
	GENERATED_BODY()
	
public:
	AExPlayerStateBase();

	/**
	 * 서버 전용: 플레이어의 점수를 추가합니다.
	 * 기본 내장된 Score 변수와 SetScore()를 활용하여 네트워크 최적화를 달성합니다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "ExPlayer")
	void AddScore(float Amount);

	/**
	 * 점수 변경 시 로컬에서 발송할 델리게이트 (HUD가 구독하여 갱신)
	 */
	UPROPERTY(BlueprintAssignable, Category = "ExPlayer")
	FOnScoreChanged OnScoreChangedDelegate;

	/**
	 * 플레이어가 매치를 시작할 준비가 되었는지 여부.
	 * 서버에서 OnPossess(빙의 완료) 시점에 설정됩니다.
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "ExMatch")
	bool bIsMatchReady = false;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	/**
	 * 네이티브 APlayerState의 OnRep_Score 가상함수를 덮어써서 
	 * 클라이언트 델리게이트를 Broadcast합니다.
	 */
	virtual void OnRep_Score() override;

private:
	// 이전 프레임의 점수를 추적하여 로컬 델리게이트에 올바른 OldScore를 제공하기 위한 캐싱 변수
	float PreviousScore;
};
