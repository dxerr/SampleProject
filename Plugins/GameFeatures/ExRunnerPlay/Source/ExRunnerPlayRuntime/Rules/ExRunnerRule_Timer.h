// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExRunnerRuleBase.h"
#include "ExRunnerRule_Timer.generated.h"

/**
 * UExRunnerRule_Timer
 * 제한 시간 룰 — 카운트다운 후 TimeUp 발동
 *
 * 서버: 초당 1회만 GameState::RemainingTimeSeconds 갱신 (대역폭 최적화)
 * 클라이언트: 수신한 정수 초를 ViewModel에서 로컬 보간으로 부드럽게 표시
 */
UCLASS(BlueprintType, EditInlineNew)
class EXRUNNERPLAYRUNTIME_API UExRunnerRule_Timer : public UExRunnerRuleBase
{
	GENERATED_BODY()

public:
	UExRunnerRule_Timer();

	/** 총 제한 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule|Timer", meta = (ClampMin = "1.0"))
	float TotalTime = 60.f;

	/** 경고 구간 시작 기준 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule|Timer", meta = (ClampMin = "0.0"))
	float WarningTime = 10.f;

	virtual void ActivateRule() override;
	virtual void DeactivateRule() override;
	virtual void TickRule(float DeltaTime) override;

private:
	/** 서버 내부 잔여 시간 (소수점 포함) */
	float ServerRemainingTime = 0.f;

	/** 마지막으로 GameState에 전송한 정수 초 (중복 전송 방지) */
	int32 LastBroadcastSecond = -1;

	/** 경고 태그 발동 여부 (최초 1회만) */
	bool bWarningBroadcasted = false;
};
