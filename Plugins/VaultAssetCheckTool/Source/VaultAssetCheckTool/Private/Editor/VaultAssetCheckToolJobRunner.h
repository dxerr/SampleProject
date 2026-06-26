// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/EngineVersionComparison.h" // ENGINE_MAJOR_VERSION
#include "Containers/Ticker.h"

class IVaultReportTask;

/** 로그 한 줄 출력. bDetailOnly=true면 "자세히" 전용 라인(항목별). */
DECLARE_DELEGATE_TwoParams(FVaultJobLog, const FString& /*Line*/, bool /*bDetailOnly*/);
/** 진행률(0~1) + 현재 단계명. */
DECLARE_DELEGATE_TwoParams(FVaultJobProgress, float /*Percent*/, const FString& /*Phase*/);
/** 작업 종료(취소/중단 여부). */
DECLARE_DELEGATE_OneParam(FVaultJobFinished, bool /*bInterrupted*/);

/**
 * 리포트 태스크들을 게임 스레드 틱 기반으로 구동하는 러너.
 *
 * - 틱마다 시간 예산(~16ms) 동안 ProcessStep을 청크 처리 → UI 응답 유지.
 * - 매 청크 취소 검사 → "정지" 즉시 반영.
 * - 주기적으로 가용 물리 메모리를 검사: 소프트 임계 미만이면 GC, 하드 임계 미만이면 안전 중단.
 * - 태스크 종료 시 CollectGarbage로 적재 에셋 해제.
 */
class FVaultAssetCheckToolJobRunner
{
public:
	~FVaultAssetCheckToolJobRunner();

	/** 태스크 목록을 출력 폴더에 대해 실행 시작. 이미 실행 중이면 무시. */
	void Start(const TArray<TSharedPtr<IVaultReportTask>>& InTasks, const FString& InOutputDir);

	/** 정지 요청(다음 청크 진입 전 중단). */
	void RequestCancel();

	bool IsRunning() const { return bRunning; }

	FVaultJobLog OnLog;
	FVaultJobProgress OnProgress;
	FVaultJobFinished OnFinished;

private:
	bool Tick(float DeltaTime);
	void Finish(bool bInterrupted);
	void Cleanup();

	/** 가용 메모리 검사. 하드 임계 미만(GC 후에도)이면 false(중단 필요). */
	bool CheckMemoryGuard();

	void Log(const FString& Line, bool bDetailOnly = false) const { OnLog.ExecuteIfBound(Line, bDetailOnly); }

	TArray<TSharedPtr<IVaultReportTask>> Tasks;
	FString OutputDir;

	int32 TaskIndex = 0;
	int32 StepIndex = 0;
	int32 LastLoggedStep = 0; // 진행 로그 스로틀용
	bool bCurrentPrepared = false;

	bool bRunning = false;
	bool bCancelRequested = false;

#if ENGINE_MAJOR_VERSION >= 5
	FTSTicker::FDelegateHandle TickerHandle;
#else
	FDelegateHandle TickerHandle;
#endif
};
