// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ExGameModeDataSet.generated.h"

/**
 * ExGameModeDataSet
 * 게임 모드별 설정 데이터셋 (예: 점수, 라운드, 규칙 등)
 */
UCLASS(BlueprintType)
class EXFRAMEWORK_API UExGameModeDataSet : public UDataAsset
{
	GENERATED_BODY()

public:
	// 예시 데이터: 최대 점수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GameMode")
	int32 MaxScore = 100;

	// 예시 데이터: 라운드 수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GameMode")
	int32 NumRounds = 3;

	// 최대 러너 좌우 이동 각도 제한 (정면 기준, 예: 45도)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GameMode|Runner")
	float MaxRunnerYawAngle = 45.0f;

	// 러너 조이스틱 Yaw 회전 민감도 (델타 크기 조절)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GameMode|Runner")
	float RunnerLookSensitivity = 1.0f;

	// 조이스틱 좌우 회전 보간 속도 (FInterpTo Speed, 낮을수록 느리고 부드럽게 복귀)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GameMode|Runner")
	float LookInterpSpeed = 8.0f;

	// 스와이프 발동 임계 비율 (터치패드 세로 크기 대비, 0.05 ~ 1.0, 기본 30%)
	// 점프(위), 슬라이드(아래) 모두 동일한 비율 적용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GameMode|Runner", meta=(ClampMin="0.05", ClampMax="1.0"))
	float SwipeActivationPercentage = 0.3f;

	// 1. 컨테이너 폰 클래스 (예: SandboxCharacter_CMC - 이동/물리 담당)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameMode")
	TSubclassOf<APawn> ContainerPawnClass;

	// 2. 로컬 플레이어 캐릭터 클래스 (예: BP_Twinblast - 비주얼/특수 로직 담당)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameMode")
	TSubclassOf<AActor> LocalPlayerClass;

	// ============================================
	// AutoRun 모드 전용 설정
	// ============================================

	/** AutoRun 모드 점프/슬라이드 연속 발동 방지 쿨다운 (초). 기본 0.3초 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameMode|Runner|AutoRun", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float AutoRunActionCooldown = 0.3f;
};

