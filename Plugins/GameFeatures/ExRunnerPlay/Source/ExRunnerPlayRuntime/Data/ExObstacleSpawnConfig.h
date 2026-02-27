// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ExObstacleSpawnConfig.generated.h"

/**
 * UExObstacleSpawnConfig
 * 장애물 생성 확률, 제한 수량, 안전 거리 등 배치와 관련된 통합 설정 데이터 애셋입니다.
 * BP에서 이 클래스를 상속받아 여러 난이도나 모드별 설정을 공유 및 교체할 수 있습니다.
 */
UCLASS(BlueprintType, Blueprintable)
class EXRUNNERPLAYRUNTIME_API UExObstacleSpawnConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 장애물 생성 확률 (0.0 ~ 1.0), 1.0이면 항상 시도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpawnProbability = 0.5f;

	/** 장애물 간 최소 안전 거리 (이전 장애물 끝점 ~ 다음 장애물 시작점 간격 제한) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	float MinSafeDistance = 200.f;

	/** 스폰에 사용할 기본 플레이어 이동 속도 (캐릭터 이동 속도 획득 실패 시 폴백) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	float DefaultRunSpeed = 600.f;

	/**
	 * 최대 활성화 장애물 수 제한 (퍼포먼스 최적화용)
	 * 0 이하면 무제한
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (ClampMin = "0"))
	int32 MaxActiveObstacles = 10;

	/** 장애물 오브젝트 풀링 사용 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	bool bUsePooling = true;
};
