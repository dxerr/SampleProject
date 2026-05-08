// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FExSpawnPlan.generated.h"

/**
 * FExSpawnPlan
 * 장애물·아이템의 "배치 의도"를 표현하는 가벼운 데이터 구조체.
 * 
 * 액터가 아닌 순수 데이터이므로 서버·클라이언트 양측에서 동일하게 산출 가능합니다.
 * 
 * 사용 규칙:
 * - 서버: RealizeObstaclePlan() 또는 RealizeItemPlan()이 이 배열을 순회하며 실제 액터를 SpawnActor.
 * - 클라이언트: 산출만 하고 실체화는 하지 않음. 디버그 해시 검증 및 JIP catch-up용.
 * 
 * 정렬 불변식: PathDistance 오름차순 정렬 상태로 산출해야 합니다.
 * (이진 탐색 전환 및 해시 안정성 확보를 위한 설계 계약)
 */
USTRUCT(BlueprintType)
struct EXCORERUNTIME_API FExSpawnPlan
{
	GENERATED_BODY()

	/** 이 플랜이 속하는 청크의 SegmentIndex (부동소수 비교 회피용 정수 키) */
	UPROPERTY(BlueprintReadOnly, Category = "SpawnPlan")
	int32 OwnerSegmentIndex = -1;

	/**
	 * 청크 시작점 기준 로컬 PathDistance 오프셋 (cm).
	 * 양측이 동일한 기준으로 스폰 위치를 산출하도록 PathDistance 스페이스로 기록한다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "SpawnPlan")
	float LocalPathOffset = 0.f;

	/** 월드 스페이스 스폰 위치 (서버가 SpawnActor 시 사용, 클라는 검증용) */
	UPROPERTY(BlueprintReadOnly, Category = "SpawnPlan")
	FVector WorldLocation = FVector::ZeroVector;

	/** 월드 스페이스 스폰 회전 */
	UPROPERTY(BlueprintReadOnly, Category = "SpawnPlan")
	FRotator WorldRotation = FRotator::ZeroRotator;

	/**
	 * 스폰할 액터 클래스.
	 * 장애물의 경우 ObstacleClass, 아이템의 경우 PickupActorClass.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "SpawnPlan")
	TSubclassOf<AActor> ActorClass;

	/**
	 * 부가 파라미터: 아이템 Z축 오프셋 (CalculateItemZ 결과 캐싱용).
	 * 장애물 Plan에서는 0으로 남겨둔다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "SpawnPlan")
	float PlacedZ = 0.f;

	/**
	 * 장애물 타입 (아이템 Plan의 Z축 배치 결정에 사용).
	 * EExObstacleType를 직접 참조하지 않도록 uint8 원시값으로 저장.
	 * 장애물 Plan에서만 유효. 아이템 Plan에서는 0(None)으로 유지.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "SpawnPlan")
	uint8 ObstacleTypeRaw = 0;

	/**
	 * 스케일 힌트 (Strategy에서 결정한 스케일 값 전파용).
	 * 기본은 (1,1,1).
	 */
	UPROPERTY(BlueprintReadOnly, Category = "SpawnPlan")
	FVector ScaleHint = FVector::OneVector;

	// ── 결정론 검증용 (선택) ──

	/**
	 * 결정론 체크용 해시 필드 (디버그 빌드에서 양측 비교용).
	 * 1차 구현에서는 0으로 두고 2차에 해시 산출 로직 추가.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "SpawnPlan|Debug")
	int32 DeterminismHash = 0;
};
