// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FExItemSpawnEntry.h"
#include "ExRunnerItemSpawnTable.generated.h"

/**
 * 러너 스테이지의 아이템 배치 규칙을 정의하는 DataAsset.
 * 코인 라인 간격, 버프 등장 확률, 장애물 타입별 배치 옵션 등을 에디터에서 설정한다.
 */
UCLASS(BlueprintType)
class EXRUNNERPLAYRUNTIME_API UExRunnerItemSpawnTable : public UDataAsset
{
	GENERATED_BODY()

public:
	// ── 코인 라인 설정 ──

	/** 코인 아이템 목록 (가중치 기반 선택) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpawnTable|Coin")
	TArray<FExItemSpawnEntry> CoinEntries;

	/** 코인 간 간격 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpawnTable|Coin", meta = (ClampMin = "50"))
	float CoinSpacing = 150.f;

	/** 최소 코인 라인 개수 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpawnTable|Coin", meta = (ClampMin = "1"))
	int32 MinCoinsPerLine = 3;

	/** 최대 코인 라인 개수 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpawnTable|Coin", meta = (ClampMin = "1"))
	int32 MaxCoinsPerLine = 10;

	/** 코인 라인이 생성될 확률 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpawnTable|Coin", meta = (ClampMin = "0", ClampMax = "1"))
	float CoinLineSpawnProbability = 0.6f;

	/** 코인 라인 중간 끊김 확률 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpawnTable|Coin", meta = (ClampMin = "0", ClampMax = "1"))
	float CoinLineBreakProbability = 0.15f;

	// ── 버프 아이템 설정 ──

	/** 버프 아이템 목록 (가중치 기반 선택) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpawnTable|Buff")
	TArray<FExItemSpawnEntry> BuffEntries;

	/** 코인 라인 중 버프가 삽입될 확률 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpawnTable|Buff", meta = (ClampMin = "0", ClampMax = "1"))
	float BuffSpawnProbability = 0.15f;

	/** 코인 라인이 없는 청크에서 버프가 단독 스폰될 확률 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpawnTable|Buff", meta = (ClampMin = "0", ClampMax = "1"))
	float BuffSoloSpawnProbability = 0.05f;

	// ── 코인/버프 배치 패턴 (Snake) ──

	/** 지그재그(뱀 패턴) 좌우 이동 적용 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpawnTable|Pattern")
	bool bUseSnakePattern = true;

	/** 최대 좌우 이동 가능 범위 (cm). 청크 너비 중심에서 양쪽 끝까지의 거리. 
	 * 코인의 콜리전 반경(약 50~100)을 감안하여 실제 도로 폭보다 적게 설정하세요. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpawnTable|Pattern", meta = (ClampMin = "0"))
	float MaxLateralOffset = 300.f;

	/** 코인 1개당 연속 스폰 시 좌우 이동 속도(Drift) (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpawnTable|Pattern", meta = (ClampMin = "0"))
	float LateralDriftPerCoin = 50.f;

	// ── 장애물 연동 배치 설정 ──

	/** 장애물 근처 아이템 최소 이격 거리 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpawnTable|Common", meta = (ClampMin = "0"))
	float MinDistanceFromObstacle = 200.f;

	/** 아이템 간 최소 배치 간격 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpawnTable|Common", meta = (ClampMin = "0"))
	float MinDistanceBetweenItems = 100.f;

	// ── 헬퍼 함수 ──

	/** 현재 속도 조건에 맞는 코인을 가중치 기반으로 랜덤 선택한다 */
	UFUNCTION(BlueprintPure, Category = "SpawnTable")
	const UExItemDefinition* SelectRandomCoin(float CurrentSpeed) const;

	/** 현재 속도 조건에 맞는 버프를 가중치 기반으로 랜덤 선택한다 */
	UFUNCTION(BlueprintPure, Category = "SpawnTable")
	const UExItemDefinition* SelectRandomBuff(float CurrentSpeed) const;

private:
	/** 가중치 기반 랜덤 선택 공통 로직 */
	static const UExItemDefinition* SelectWeightedRandom(const TArray<FExItemSpawnEntry>& Entries, float CurrentSpeed);
};
