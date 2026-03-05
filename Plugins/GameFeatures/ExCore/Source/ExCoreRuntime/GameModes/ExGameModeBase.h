// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayTagContainer.h"
#include "ExGameModeBase.generated.h"

/**
 * ExCore의 기본 GameMode 클래스입니다.
 * 서버 권한으로 게임 매치의 진행(Phase) 및 플레이어의 접속(PostLogin)을 관장합니다.
 */
UCLASS()
class EXCORERUNTIME_API AExGameModeBase : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	AExGameModeBase();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * 서버에서 새로운 매치 페이즈를 설정하고 GameState에 전파합니다.
	 * (주의: 허용된 매치 상태 전이 맵을 내장하여 유효성을 검사합니다)
	 */
	UFUNCTION(BlueprintCallable, Category = "ExMatch")
	void SetMatchPhase(FGameplayTag NewPhase, bool bForceTransition = false);

	/** 모든 플레이어 컨트롤러가 로딩을 성공적으로 끝냈는지 체크 */
	UFUNCTION(BlueprintPure, Category = "ExMatch")
	bool CheckAllPlayersReady() const;

	/**
	 * UExGameFlowSubsystem의 Travel 요청 이벤트를 받는 콜백.
	 * 여기에서 GetWorld()->ServerTravel을 직접 호출하여 맵 오버라이딩을 수행합니다.
	 */
	UFUNCTION()
	void OnFlowSubsystemRequestTravel(const FString& MapURL);

protected:
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/** 매치 시작 로직을 위한 가상 함수. 하위 GameMode에서 재정의합니다. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "ExMatch")
	void OnMatchStarted();
	virtual void OnMatchStarted_Implementation();

	/** 매치 종료 결과를 처리하기 위한 가상 함수. 하위 GameMode에서 재정의합니다. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "ExMatch")
	void OnMatchEnded();
	virtual void OnMatchEnded_Implementation();

	/** 매치 진행 허용 전이 루프 정보 */
	TMap<FGameplayTag, TArray<FGameplayTag>> AllowedMatchTransitions;
};
