// Copyright ExFrameWork. All Rights Reserved.
// 파일: FExPreloadOptions.h
// 목적: UExAssetPreloadSubsystem의 프리로드 요청 옵션 구조체
// 작성: Antigravity
// 생성일: 2026-05-19

#pragma once

#include "CoreMinimal.h"
#include "FExPreloadOptions.generated.h"

/**
 * FExPreloadOptions
 * 비동기 프리로드 요청 시 전달하는 옵션 구조체.
 * 로딩 우선순위 등 프리로드 동작을 제어한다.
 */
USTRUCT(BlueprintType)
struct EXCORERUNTIME_API FExPreloadOptions
{
	GENERATED_BODY()

	/** 로딩 스레드 우선순위 (0: 최하위 ~ 100: 최상위, 기본값 0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preload")
	int32 LoadPriority = 0;
};
