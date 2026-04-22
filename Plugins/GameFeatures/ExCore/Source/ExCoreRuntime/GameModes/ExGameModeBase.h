// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayTagContainer.h"
#include "ExGameModeBase.generated.h"

class UExCoreSpawnDataAsset;

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
	virtual void Tick(float DeltaTime) override;

	// ========== 스폰 관리 (Spawn & Visual Override) ==========

	/**
	 * 스폰 설정 데이터 에셋
	 * PawnClasses, VisualOverrides 등을 데이터 드리븐 방식으로 관리
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ExMatch|Spawn")
	TObjectPtr<UExCoreSpawnDataAsset> SpawnDataAsset;

	// ========== 경험 관리 (UI & 플로우) ==========

	/**
	 * 이 게임 모드가 시작될 때 기본적으로 활성화할 경험(UI 세팅 등) 데이터입니다.
	 * 블루프린트 디테일 패널에서 에셋을 할당해 주면 자동으로 적용됩니다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ExMatch|Experience")
	TObjectPtr<const class UExExperienceDefinition> DefaultExperience;

	/**
	 * 서버 권한으로 런타임에 Visual Override 변경
	 * @param TargetPawn 대상 폰
	 * @param NewVisualIndex 새 Visual 인덱스
	 */
	UFUNCTION(BlueprintCallable, Category = "ExMatch|Spawn")
	void ChangeVisualOverride(APawn* TargetPawn, int32 NewVisualIndex);



	// ========== 매치 흐름 관리 ==========

	/** 
	 * 로딩이 완료된 시점에 자동으로 Match_Playing 상태로 진입할지 여부 
	 * (Runner 등 바로 시작해야 하는 모드에서 true로 사용)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExMatch|Flow")
	bool bAutoStartOnReady;

	/**
	 * 서버에서 새로운 매치 페이즈를 설정하고 GameState에 전파합니다.
	 * (주의: 허용된 매치 상태 전이 맵을 내장하여 유효성을 검사합니다)
	 */
	// 매치 플로우
	UFUNCTION(BlueprintCallable, Category = "ExMatch|Flow")
	void SetMatchPhase(FGameplayTag NewPhase, bool bForceTransition = false);

	UFUNCTION(BlueprintCallable, Category = "ExMatch|Flow")
	virtual void CheckAndStartMatch();

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

	/**
	 * 기본 폰 스폰 - 데이터 에셋 기반 커스텀 스폰 로직
	 * @param NewPlayer 새 플레이어 컨트롤러
	 * @param SpawnTransform 스폰 위치/회전
	 * @return 스폰된 폰
	 */
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

	/** 매치 시작 로직을 위한 가상 함수. 하위 GameMode에서 재정의합니다. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "ExMatch")
	void OnMatchStarted();
	virtual void OnMatchStarted_Implementation();

	/** 매치 종료 결과를 처리하기 위한 가상 함수. 하위 GameMode에서 재정의합니다. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "ExMatch")
	void OnMatchEnded();
	virtual void OnMatchEnded_Implementation();

};
