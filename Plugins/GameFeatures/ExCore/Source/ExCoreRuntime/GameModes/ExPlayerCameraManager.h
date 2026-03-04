// Copyright ExFrameWork. All Rights Reserved.
// ExCore 기본 CameraManager — SkyDome 등 뷰포트에 붙어다녀야 하는 시각 요소 관리

#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "ExPlayerCameraManager.generated.h"

/**
 * AExPlayerCameraManager
 * ExFrameWork 기본 플레이어 카메라 매니저.
 * 
 * 주요 기능:
 * - 거대 배경 모델(예: SkyDome)을 카메라 위치에 동기화하여 렌더링 최적화 및 정밀도 문제 해결
 */
UCLASS()
class EXCORERUNTIME_API AExPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

public:
	AExPlayerCameraManager();

	virtual void BeginPlay() override;

	// 매 프레임 카메라 위치 및 회전이 결정된 직후 호출
	virtual void UpdateCamera(float DeltaTime) override;

	/** 현재 매니저가 따라다니게 할 타겟 스카이돔 지정 */
	UFUNCTION(BlueprintCallable, Category = "Ex|Camera")
	void SetTargetSkyDome(AActor* InSkyDome);

protected:
	/** 
	 * 맵에서 자동으로 찾을 스카이돔의 액터 태그(Actor Tag)입니다. 
	 * 레벨에 배치된 스카이돔 액터의 디테일 패널 -> Actor -> Tags 에 이 이름(기본값: "SkyDome")을 추가해두면 BeginPlay 시 자동으로 찾아 등록합니다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Ex|SkyDome")
	FName TargetSkyDomeTag = FName("SkyDome");

	/** 현재 씬에 배치된 스카이돔 액터 참조 */
	UPROPERTY(Transient)
	TObjectPtr<AActor> TargetSkyDome;

	/** 
	 * 스카이돔의 위치 동기화 축 설정
	 * true일 경우 X, Y만 동기화하고 Z값은 초기값을 유지합니다. (일반적인 스카이돔 방식)
	 */
	UPROPERTY(EditAnywhere, Category = "Ex|SkyDome")
	bool bFollowXYOnly = true;

	/** bFollowXYOnly가 true일 때 고정할 Z축 높이 */
	UPROPERTY()
	float InitialSkyDomeZ = 0.0f;
};
