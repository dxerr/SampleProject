// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "../Data/Modes/ExGameModeDataSet.h"
#include "ExGameMode.generated.h"

/**
 * ExGameMode
 * ExFrameWork 프로젝트의 기본 게임 모드 클래스
 */
UCLASS()
class EXFRAMEWORK_API AExGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AExGameMode();

	/**
	 * 게임 모드 데이터셋 (에디터에서 설정 가능)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GameMode")
	UExGameModeDataSet* GameModeDataSet;

protected:
	virtual void BeginPlay() override;

public:
	UFUNCTION(BlueprintCallable, Category = "GameMode")
	void SpawnLocalPlayer(AController* NewPlayer);

	virtual void Tick(float DeltaTime) override;
};
