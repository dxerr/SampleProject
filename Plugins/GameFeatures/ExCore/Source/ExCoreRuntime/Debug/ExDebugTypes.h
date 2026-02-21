// Copyright ExFrameWork. All Rights Reserved.
// 디버그 시스템 공용 열거형 및 구조체 정의

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ExDebugTypes.generated.h"

/**
 * 치트 실행 타입
 * DataAsset에서 치트 종류를 구분하기 위해 사용
 */
UENUM(BlueprintType)
enum class EExCheatType : uint8
{
	/** On/Off 토글 */
	Toggle		UMETA(DisplayName = "토글"),
	/** 일회성 실행 */
	OneShot		UMETA(DisplayName = "원샷"),
	/** 슬라이더(float 범위) */
	Slider		UMETA(DisplayName = "슬라이더"),
	/** 선택형(enum/목록) */
	Select		UMETA(DisplayName = "선택")
};

/**
 * FExDebugCheatState
 * 개별 치트의 런타임 상태를 저장하는 구조체
 */
USTRUCT(BlueprintType)
struct EXCORERUNTIME_API FExDebugCheatState
{
	GENERATED_BODY()

	/** 치트 활성화 여부 (Toggle 타입용) */
	UPROPERTY(BlueprintReadWrite, Category = "Ex|Debug")
	bool bEnabled = false;

	/** 치트 수치 (Slider 타입용) */
	UPROPERTY(BlueprintReadWrite, Category = "Ex|Debug")
	float Value = 0.f;

	/** 선택 인덱스 (Select 타입용) */
	UPROPERTY(BlueprintReadWrite, Category = "Ex|Debug")
	int32 SelectedIndex = 0;
};
