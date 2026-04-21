// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Player/ExPlayerStateBase.h"
#include "ExRunnerPlayerState.generated.h"

/**
 * AExRunnerPlayerState
 * 러너 멀티플레이에서 각 플레이어의 고유 런타임 상태(거리 등)를 
 * 서버 권한으로 기록하고 클라이언트에 복제(Replicate)하는 특수 PlayerState입니다.
 */
UCLASS()
class EXRUNNERPLAYRUNTIME_API AExRunnerPlayerState : public AExPlayerStateBase
{
	GENERATED_BODY()

public:
	AExRunnerPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 서버 권한으로 기록되는 해당 플레이어의 누적 주행 거리 */
	UPROPERTY(Replicated, Transient, VisibleAnywhere, BlueprintReadOnly, Category = "ExRunner|PlayerState")
	float ServerAuthPathDistance;

	/** 로컬 전용: 클라이언트가 예측이나 부드러운 카메라 보간을 위해 사용하는 가상 거리 (직렬화 X) */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "ExRunner|PlayerState")
	float ClientPredictedPathDistance;

	/** 서버에서 이 플레이어의 거리를 갱신할 때 호출 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "ExRunner|PlayerState")
	void UpdatePathDistance(float NewDistance);
};
