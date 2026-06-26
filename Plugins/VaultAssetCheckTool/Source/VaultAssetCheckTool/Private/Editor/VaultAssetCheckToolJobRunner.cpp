// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/VaultAssetCheckToolJobRunner.h"
#include "Editor/VaultAssetCheckToolReportTask.h"

#include "HAL/PlatformTime.h"
#include "HAL/PlatformMemory.h"
#include "UObject/UObjectGlobals.h" // CollectGarbage, GARBAGE_COLLECTION_KEEPFLAGS

#if ENGINE_MAJOR_VERSION >= 5
using FVaultTicker = FTSTicker;
#else
using FVaultTicker = FTicker;
#endif

namespace
{
	// 한 틱에 처리할 시간 예산(초). 이 시간 동안 ProcessStep을 반복하고 프레임에 제어를 돌려준다.
	constexpr double kTickBudgetSeconds = 0.016;
	// 메모리 검사 주기(스텝 수).
	constexpr int32 kMemoryCheckInterval = 128;
	// 진행 로그 출력 주기(스텝 수).
	constexpr int32 kProgressLogInterval = 2000;
	// 가용 물리 메모리 임계(전체 물리 대비 비율).
	constexpr double kSoftMemoryRatio = 0.10; // 이 아래면 GC
	constexpr double kHardMemoryRatio = 0.05; // GC 후에도 이 아래면 중단
}

FVaultAssetCheckToolJobRunner::~FVaultAssetCheckToolJobRunner()
{
	Cleanup();
}

void FVaultAssetCheckToolJobRunner::Start(const TArray<TSharedPtr<IVaultReportTask>>& InTasks, const FString& InOutputDir)
{
	if (bRunning || InTasks.Num() == 0)
	{
		return;
	}

	Tasks = InTasks;
	OutputDir = InOutputDir;
	TaskIndex = 0;
	StepIndex = 0;
	bCurrentPrepared = false;
	bRunning = true;
	bCancelRequested = false;

	Log(FString::Printf(TEXT("=== 작업 시작 (%d개 리포트) → %s"), Tasks.Num(), *OutputDir));
	OnProgress.ExecuteIfBound(0.0f, TEXT(""));

	TickerHandle = FVaultTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FVaultAssetCheckToolJobRunner::Tick), 0.0f);
}

void FVaultAssetCheckToolJobRunner::RequestCancel()
{
	if (bRunning && !bCancelRequested)
	{
		bCancelRequested = true;
		Log(TEXT("정지 요청됨 — 현재 청크 처리 후 중단합니다."));
	}
}

bool FVaultAssetCheckToolJobRunner::CheckMemoryGuard()
{
	const FPlatformMemoryConstants& MemConstants = FPlatformMemory::GetConstants();
	const uint64 TotalPhysical = MemConstants.TotalPhysical;
	if (TotalPhysical == 0)
	{
		return true; // 알 수 없으면 통과
	}

	FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
	const uint64 SoftThreshold = static_cast<uint64>(TotalPhysical * kSoftMemoryRatio);
	const uint64 HardThreshold = static_cast<uint64>(TotalPhysical * kHardMemoryRatio);

	if (Stats.AvailablePhysical >= SoftThreshold)
	{
		return true;
	}

	// 소프트 임계 미만 → GC 후 재측정
	Log(FString::Printf(TEXT("  · 가용 메모리 낮음(%llu MB) → GC 수행"), Stats.AvailablePhysical / (1024 * 1024)));
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, /*bPerformFullPurge=*/true);

	Stats = FPlatformMemory::GetStats();
	if (Stats.AvailablePhysical < HardThreshold)
	{
		Log(FString::Printf(TEXT("  · GC 후에도 가용 메모리 부족(%llu MB < 하드 임계) → 안전 중단"),
			Stats.AvailablePhysical / (1024 * 1024)));
		return false;
	}

	return true;
}

bool FVaultAssetCheckToolJobRunner::Tick(float /*DeltaTime*/)
{
	if (bCancelRequested)
	{
		Log(FString::Printf(TEXT("=== 사용자 정지 (%d/%d 리포트 완료)"), TaskIndex, Tasks.Num()));
		Finish(/*bInterrupted=*/true);
		return false;
	}

	if (!Tasks.IsValidIndex(TaskIndex))
	{
		Log(TEXT("=== 작업 완료"));
		Finish(/*bInterrupted=*/false);
		return false;
	}

	TSharedPtr<IVaultReportTask> Task = Tasks[TaskIndex];
	const FString LabelStr = Task->GetLabel().ToString();

	// 준비 단계(에셋 수집)
	if (!bCurrentPrepared)
	{
		Task->Prepare(OutputDir);
		bCurrentPrepared = true;
		StepIndex = 0;
		LastLoggedStep = 0;
		Log(FString::Printf(TEXT("▶ [%d/%d] %s 시작 (%d개 항목)"),
			TaskIndex + 1, Tasks.Num(), *LabelStr, Task->NumSteps()));
	}

	const int32 Total = Task->NumSteps();
	const double TickStart = FPlatformTime::Seconds();

	// 시간 예산 동안 청크 처리
	while (StepIndex < Total)
	{
		if (bCancelRequested)
		{
			break; // 다음 틱 상단에서 정지 처리
		}

		if ((StepIndex % kMemoryCheckInterval) == 0 && !CheckMemoryGuard())
		{
			// 메모리 부족 → 지금까지 누적분을 저장하고 이 작업 전체를 중단
			Task->Finalize();
			Log(FString::Printf(TEXT("[중단] %s — 메모리 부족으로 부분 저장 후 종료"), *LabelStr));
			Finish(/*bInterrupted=*/true);
			return false;
		}

		Task->ProcessStep(StepIndex++);

		// 항목별 로그는 항상 "자세히 전용"으로 기록(표시 여부는 패널의 자세히 토글이 결정)
		Log(FString::Printf(TEXT("  · [%d/%d] %s"), StepIndex, Total, *Task->GetStepLabel(StepIndex - 1)), /*bDetailOnly=*/true);

		if ((FPlatformTime::Seconds() - TickStart) >= kTickBudgetSeconds)
		{
			break; // 프레임에 제어 반환
		}
	}

	// 진행률 갱신: (완료 리포트 + 현재 리포트 진행분) / 전체 리포트
	const float TaskFraction = (Total > 0) ? (static_cast<float>(StepIndex) / static_cast<float>(Total)) : 1.0f;
	const float Overall = (static_cast<float>(TaskIndex) + TaskFraction) / static_cast<float>(Tasks.Num());
	OnProgress.ExecuteIfBound(Overall, FString::Printf(TEXT("%s (%d/%d)"), *LabelStr, StepIndex, Total));

	// 진행 요약 로그(간단 로그용 하트비트): 2000건마다 1줄. 두 버퍼 모두에 기록.
	if (StepIndex < Total && (StepIndex - LastLoggedStep) >= kProgressLogInterval)
	{
		LastLoggedStep = StepIndex;
		const int32 Pct = (Total > 0) ? FMath::RoundToInt(TaskFraction * 100.0f) : 0;
		Log(FString::Printf(TEXT("  · %s 처리 중 %d/%d (%d%%)"), *LabelStr, StepIndex, Total, Pct));
	}

	// 현재 리포트 완료 → 저장 + GC + 다음으로
	if (StepIndex >= Total)
	{
		const double SaveStart = FPlatformTime::Seconds();
		const bool bOk = Task->Finalize();
		Log(FString::Printf(TEXT("%s %s (%.1f초)"),
			bOk ? TEXT("[성공]") : TEXT("[실패]"), *LabelStr, FPlatformTime::Seconds() - SaveStart));

		// 적재된 에셋 해제(리포트 간 누적 방지)
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, /*bPerformFullPurge=*/true);

		++TaskIndex;
		bCurrentPrepared = false;
		StepIndex = 0;
	}

	return true; // 계속
}

void FVaultAssetCheckToolJobRunner::Finish(bool bInterrupted)
{
	Cleanup();
	OnProgress.ExecuteIfBound(bInterrupted ? 0.0f : 1.0f, TEXT(""));
	OnFinished.ExecuteIfBound(bInterrupted);
}

void FVaultAssetCheckToolJobRunner::Cleanup()
{
	if (TickerHandle.IsValid())
	{
		FVaultTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
	Tasks.Reset();
	bRunning = false;
}
