// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * ExNetwork 플러그인 전용 로그 카테고리.
 * 모든 ExNetwork 코드는 이 카테고리로 로그를 출력한다.
 *
 * 사용 예:
 *   UE_LOG(LogExNetwork, Log, TEXT("ExOnlineSubsystem Initialized"));
 *   UE_LOG(LogExNetwork, Warning, TEXT("Login pending..."));
 *   UE_LOG(LogExNetwork, Error, TEXT("Match creation failed"));
 *
 * DEFINE_LOG_CATEGORY(LogExNetwork) 는 ExNetworkRuntimeModule.cpp 에 배치된다.
 */
EXNETWORKRUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogExNetwork, Log, All);
