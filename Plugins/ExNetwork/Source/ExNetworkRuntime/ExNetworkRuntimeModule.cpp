// Copyright ExFrameWork. All Rights Reserved.

#include "ExNetworkRuntimeModule.h"
#include "Core/ExNetworkLog.h"
#include "Modules/ModuleManager.h"

// ExNetwork 플러그인 전용 로그 카테고리 정의.
// 선언(DECLARE)은 Core/ExNetworkLog.h 에 있고, 정의(DEFINE)는 여기 모듈 cpp에 배치한다.
DEFINE_LOG_CATEGORY(LogExNetwork);

void FExNetworkRuntimeModule::StartupModule()
{
	UE_LOG(LogExNetwork, Log, TEXT("[ExNetworkRuntime] StartupModule — ExNetwork Phase 1 플러그인 로드 완료."));
}

void FExNetworkRuntimeModule::ShutdownModule()
{
	UE_LOG(LogExNetwork, Log, TEXT("[ExNetworkRuntime] ShutdownModule — ExNetwork 플러그인 언로드."));
}

IMPLEMENT_MODULE(FExNetworkRuntimeModule, ExNetworkRuntime)
