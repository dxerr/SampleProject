// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "ExObstacleInteractionComponent.generated.h"

/**
 * 장애물 상호작용 컴포넌트
 * 플레이어와 겹쳤을 때 러너 무브먼트의 상호작용 타겟(Warp)을 설정합니다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class EXCORERUNTIME_API UExObstacleInteractionComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UExObstacleInteractionComponent();

	/**
	 * 모션 워핑 타겟 포인트 이름 (예: "ClimbPoint", "VaultPoint")
	 * 장애물 액터 내에 해당 이름의 SceneComponent가 있어야 함 (없으면 액터 Root 기준)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FName WarpTargetName = FName("ClimbPoint");

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
