// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ExCoreGameMode.generated.h"

class UExCoreSpawnDataAsset;

/**
 * AExCoreGameMode
 * ExCore GameFeature 플러그인의 독립적인 게임 모드
 * GM_Sandbox 스타일의 컨테이너 폰 + Visual Override 스폰 시스템 구현
 */
UCLASS()
class EXCORERUNTIME_API AExCoreGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AExCoreGameMode();

	/**
	 * 스폰 설정 데이터 에셋
	 * PawnClasses, VisualOverrides 등을 데이터 드리븐 방식으로 관리
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	TObjectPtr<UExCoreSpawnDataAsset> SpawnDataAsset;

protected:
	virtual void BeginPlay() override;

	/**
	 * 새 플레이어 처리 - 스폰 프로세스 시작점
	 */
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	/**
	 * 기본 폰 스폰 - 커스텀 스폰 로직의 핵심
	 * @param NewPlayer 새 플레이어 컨트롤러
	 * @param SpawnTransform 스폰 위치/회전
	 * @return 스폰된 폰
	 */
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

public:
	/**
	 * 컨테이너 폰에 Visual Override를 적용
	 * @param ContainerPawn 대상 컨테이너 폰
	 * @param VisualClass 적용할 Visual Override 클래스
	 * @return 생성된 Visual Actor
	 */
	UFUNCTION(BlueprintCallable, Category = "Spawn")
	AActor* ApplyVisualOverride(APawn* ContainerPawn, TSubclassOf<AActor> VisualClass);

	/**
	 * 런타임에 Visual Override 변경
	 * @param TargetPawn 대상 폰
	 * @param NewVisualIndex 새 Visual 인덱스
	 */
	UFUNCTION(BlueprintCallable, Category = "Spawn")
	void ChangeVisualOverride(APawn* TargetPawn, int32 NewVisualIndex);

private:
	/**
	 * 현재 스폰된 Visual Actor들 추적
	 * Key: 컨테이너 폰, Value: 부착된 Visual Actor
	 */
	UPROPERTY()
	TMap<APawn*, AActor*> SpawnedVisualActors;
};
