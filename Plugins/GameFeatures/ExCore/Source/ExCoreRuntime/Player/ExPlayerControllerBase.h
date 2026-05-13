// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ExPlayerControllerBase.generated.h"

class UExExperienceDefinition;

/**
 * 프로젝트 전역 베이스 PlayerController
 * Experience Manager(UI 비동기 로드 등) 로드 완료 시 
 * 서버에 세션 진입 준비가 끝났음을 알리는 역할을 포함합니다.
 */
UCLASS()
class EXCORERUNTIME_API AExPlayerControllerBase : public APlayerController
{
	GENERATED_BODY()

public:
	AExPlayerControllerBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PostProcessInput(const float DeltaTime, const bool bGamePaused) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 로컬 플레이어가 배정되어 뷰포트 접근이 가능해지는 시점
	virtual void ReceivedPlayer() override;
	virtual void OnPossess(APawn* aPawn) override;
	virtual void PostSeamlessTravel() override;
	virtual void PlayerTick(float DeltaTime) override;

protected:
	// Experience Manager 바인딩 시도 함수
	void TryBindExperienceManager();

	// GameState의 ExperienceManager 로드 완료 이벤트를 수신하는 콜백
	UFUNCTION()
	void OnExperienceLoadComplete();

	// 클라이언트가 UI 및 에셋 로딩을 마쳤음을 서버에 알림
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_NotifyReadyForMatch();

public:
	/**
	 * [Play 버튼용 Server RPC]
	 * 클라이언트에서 호출 → 서버에서 ExGameFlowSubsystem::TransitionToExperience 실행.
	 * 데디케이티드 서버 환경에서 클라이언트가 직접 TransitionToExperience를
	 * 호출하면 클라이언트에는 GameMode가 없어 OnRequestTravel이 아무것도 안 함.
	 * 이 RPC를 통해 반드시 서버 측 GameMode가 ServerTravel을 실행하도록 합니다.
	 */
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ExFlow",
		meta = (DisplayName = "[Server] Request Transition To Experience"))
	void Server_RequestTransitionToExperience(const UExExperienceDefinition* ExperienceConfig);

	/** Late Join 플레이어에게 매치 참여 불가 팝업 표시 */
	UFUNCTION(Client, Reliable)
	void Client_ShowLateJoinPopup();

private:
	bool bWaitingForGameState = false;
};
