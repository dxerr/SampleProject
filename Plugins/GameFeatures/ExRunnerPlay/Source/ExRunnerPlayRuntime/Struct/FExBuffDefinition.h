// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FExBuffDefinition.generated.h"

/**
 * 버프 타입 열거형
 * 새 버프 추가 시 여기에 타입을 먼저 정의합니다.
 */
UENUM(BlueprintType)
enum class EExBuffType : uint8
{
	SpeedUp   UMETA(DisplayName = "속도 증가"),
	SpeedDown UMETA(DisplayName = "속도 감소"),
};

/**
 * 버프 정의 구조체
 * 아이템이 발행하는 이벤트 페이로드나 아이템 데이터 에셋에 포함되어
 * 어떤 버프를 얼마나 강하게, 얼마 동안 적용할지 명세합니다.
 */
USTRUCT(BlueprintType)
struct EXRUNNERPLAYRUNTIME_API FExBuffDefinition
{
	GENERATED_BODY()

	/** 버프 타입 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buff")
	EExBuffType BuffType = EExBuffType::SpeedUp;

	/** 지속 시간 (초). 0 이하이면 즉시 만료 처리 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buff", meta = (ClampMin = "0.1"))
	float Duration = 5.0f;

	/**
	 * 가중치 (배율).
	 * SpeedUp 의 경우: BaseMaxSpeed × Weight 로 이동 속도 결정 (예: 1.3 = 130%)
	 * SpeedDown 의 경우: 미사용
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buff", meta = (ClampMin = "0.1"))
	float Weight = 1.3f;

	/**
	 * 이 버프 활성화 시 먼저 제거(종료)할 버프 목록.
	 * 예: SpeedUp 의 RemoveList = {SpeedDown} → SpeedDown 이 걸린 상태에서 SpeedUp 을 먹으면 SpeedDown 먼저 해제
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buff")
	TArray<EExBuffType> RemoveList;
};

/**
 * 런타임에 실제로 활성화된 버프 상태.
 * ExRunnerBuffComponent 내부에서만 사용합니다.
 */
USTRUCT()
struct FExActiveBuffState
{
	GENERATED_BODY()

	UPROPERTY()
	EExBuffType BuffType = EExBuffType::SpeedUp;

	/** 서버에서 관리하는 잔여 시간 (초) */
	UPROPERTY()
	float RemainingTime = 0.0f;

	/** 적용된 가중치 (복구 시 1.0f 로 되돌리기 위해 보관) */
	UPROPERTY()
	float Weight = 1.0f;

	bool operator==(const FExActiveBuffState& Other) const { return BuffType == Other.BuffType; }
};
