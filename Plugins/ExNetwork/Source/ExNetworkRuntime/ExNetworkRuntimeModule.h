// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/**
 * FExNetworkRuntimeModule
 *
 * ExNetworkRuntime 모듈의 진입점 클래스.
 * ExCore의 FExCoreRuntimeModule 패턴을 답습한다.
 *
 * Phase 1: 모듈 로드/언로드 로그 출력 및 LogExNetwork 카테고리 등록.
 * Phase 2 이후: EOS SDK 초기화 등이 StartupModule에 추가될 수 있다.
 */
class FExNetworkRuntimeModule : public IModuleInterface
{
public:

	/** 모듈 로드 시 호출. LogExNetwork 카테고리 정의 및 초기화 로그 출력. */
	virtual void StartupModule() override;

	/** 모듈 언로드 시 호출. */
	virtual void ShutdownModule() override;
};
