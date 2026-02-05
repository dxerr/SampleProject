// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExCoreGameMode.h"
#include "ExRunnerGameMode.generated.h"

class UExChunkSpawner;
class UExObstacleManager;

/**
 * AExRunnerGameMode
 * 러너 게임 전용 모드
 * 트레드밀 시스템, 장애물 관리, 게임 속도 제어 등을 담당
 * 비주얼 오버라이드 기능은 부모인 ExCoreGameMode에서 상속받음
 */
UCLASS()
class EXRUNNERPLAYRUNTIME_API AExRunnerGameMode : public AExCoreGameMode
{
	GENERATED_BODY()

public:
	AExRunnerGameMode();

	// ========== 러너 게임 시스템 ==========
public:
	/**
	 * 러너 게임 기본 속도 (cm/s)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Runner")
	float BaseGameSpeed = 600.f;

	/**
	 * 초당 속도 가속률 (cm/s²)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Runner")
	float SpeedAcceleration = 10.f;

	/**
	 * 현재 게임 속도 (런타임)
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Runner")
	float CurrentGameSpeed = 0.f;

	/**
	 * 총 이동 거리 (점수/거리 매칭용)
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Runner")
	float TotalDistance = 0.f;

	/**
	 * 러너 모드 활성화 여부
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Runner")
	bool bRunnerModeEnabled = true;

	/**
	 * 트레드밀 일시 정지 여부 (장애물 등반 시 true)
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Runner")
	bool bTreadmillPaused = false;

	/**
	 * 트레드밀 일시 정지 설정
	 */
	UFUNCTION(BlueprintCallable, Category = "Runner")
	void SetTreadmillPaused(bool bPaused);

	/**
	 * 현재 게임 속도 반환
	 */
	UFUNCTION(BlueprintPure, Category = "Runner")
	float GetCurrentGameSpeed() const { return CurrentGameSpeed; }

	/**
	 * 러너 게임 시작
	 */
	UFUNCTION(BlueprintCallable, Category = "Runner")
	void StartRunnerGame();

	/**
	 * 러너 게임 중지
	 */
	UFUNCTION(BlueprintCallable, Category = "Runner")
	void StopRunnerGame();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	/**
	 * 청크 스포너 컴포넌트
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UExChunkSpawner* ChunkSpawner;

	/**
	 * 장애물 매니저 컴포넌트
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UExObstacleManager* ObstacleManager;
};
