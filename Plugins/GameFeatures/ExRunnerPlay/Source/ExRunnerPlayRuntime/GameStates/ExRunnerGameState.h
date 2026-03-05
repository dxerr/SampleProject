// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameModes/ExGameStateBase.h"
#include "ExRunnerGameState.generated.h"

class UExPathManager;

/**
 * AExRunnerGameState
 * 러너 모드 전용 게임 상태 클래스
 * 서버와 클라이언트 모두에서 경로(PathManager)와 이동 거리에 접근 가능하도록 동기화합니다.
 */
UCLASS()
class EXRUNNERPLAYRUNTIME_API AExRunnerGameState : public AExGameStateBase
{
	GENERATED_BODY()

public:
	AExRunnerGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 현재 트레드밀의 누적 이동 거리 (가상 거리)
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "ExRunner|Status")
	float CurrentPathDistance = 0.f;

	// 실제 플레이어의 경로상 위치 (Chunk 삭제 및 곡선 산출용)
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "ExRunner|Status")
	float RealPlayerPathDistance = 0.f;

	UFUNCTION(BlueprintCallable, Category = "ExRunner|Status")
	float GetCurrentPathDistance() const { return CurrentPathDistance; }

	UFUNCTION(BlueprintCallable, Category = "ExRunner|Status")
	float GetPlayerPathDistance() const { return RealPlayerPathDistance; }

	// 클라이언트 클라이언트 모두 접근할 경로 매니저 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UExPathManager> PathManager;
};
