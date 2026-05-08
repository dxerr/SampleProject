// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/Base/ExDefinitionDataAsset.h"
#include "Misc/DataValidation.h"
#include "GameFramework/Actor.h"
#include "ExObstacleDefinition.generated.h"

/**
 * 장애물 통과 유형 정의
 */
UENUM(BlueprintType)
enum class EExObstacleType : uint8
{
	None,
	Gap,		// 점프해서 건너 뛰기 (바닥 없음)
	WallRun,	// 벽 타고 달리기
	Climb,		// 매달려서 올라가기 (머리 높이)
	Slide		// 슬라이딩 (낮은 틈)
};

/**
 * Runner Game의 장애물 메타데이터 정의
 */
UCLASS(BlueprintType, Blueprintable)
class EXRUNNERPLAYRUNTIME_API UExObstacleDefinition : public UExDefinitionDataAsset
{
	GENERATED_BODY()

public:
	// 스폰할 실제 액터 클래스
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Obstacle")
	TSubclassOf<AActor> ObstacleClass;

	// 장애물의 크기 범위 (랜덤 생성용)
	// X: 길이, Z: 높이 (Y는 바닥 폭에 맞춰 자동 조정되므로 무시됨)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Obstacle")
	FVector MinSize = FVector(100.f, 1000.f, 100.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Obstacle")
	FVector MaxSize = FVector(200.f, 1000.f, 150.f);

	// 통과 방식
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Obstacle")
	EExObstacleType Type;

	// 통과 후 복귀(회복) 시간 (초 단위)
	// 예: 높은 벽을 넘은 후 착지하여 다시 달리기 시작할 때까지의 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Logic")
	float RecoveryTime = 1.0f;

	// 통과를 위해 필요한 최소 진입 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Logic")
	float MinEntrySpeed = 400.0f;

	// 배치 시 바닥에서의 Z 오프셋 (공중 장애물 등)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	float VerticalOffset = 0.0f;

	// ── Gap 타입 전용 ──
	// Gap 구간에서 바닥 메시를 숨길지 여부
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Type|Gap",
		meta = (EditCondition = "Type == EExObstacleType::Gap", EditConditionHides))
	bool bDisableFloorMesh = true;

	// ── WallRun 타입 전용 ──
	// 벽 달리기 구간의 벽 높이 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Type|WallRun",
		meta = (EditCondition = "Type == EExObstacleType::WallRun", EditConditionHides))
	float WallHeight = 300.f;

	// ── Climb 타입 전용 ──
	// 매달려 올라가는 구간의 높이 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Type|Climb",
		meta = (EditCondition = "Type == EExObstacleType::Climb", EditConditionHides))
	float ClimbHeight = 400.f;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override
	{
		bool bIsErrorEmpty = true;
		if (!ObstacleClass)
		{
			Context.AddError(FText::FromString(TEXT("ObstacleClass가 비어 있습니다.")));
			bIsErrorEmpty = false;
		}

		return bIsErrorEmpty ? Super::IsDataValid(Context) : EDataValidationResult::Invalid;
	}
#endif
};
