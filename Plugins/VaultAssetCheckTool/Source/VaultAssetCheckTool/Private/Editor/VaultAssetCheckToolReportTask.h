// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * 리포트 1건을 "에셋 단위 스텝"으로 실행 가능하게 추상화한 인터페이스.
 *
 * 러너(FVaultAssetCheckToolJobRunner)가 게임 스레드 틱에서:
 *   Prepare() → ProcessStep(0..NumSteps-1) [청크 단위] → Finalize()
 * 순으로 구동한다. UObject 로드/추출은 모두 게임 스레드에서 일어난다.
 */
class IVaultReportTask
{
public:
	virtual ~IVaultReportTask() {}

	/** 표시 라벨. */
	virtual FText GetLabel() const = 0;

	/** 게임 스레드: 에셋 목록 수집 등 사전 준비. */
	virtual void Prepare(const FString& OutputDir) = 0;

	/** 총 스텝 수(처리할 에셋 수). Prepare 이후 유효. */
	virtual int32 NumSteps() const = 0;

	/** 게임 스레드: Index번째 에셋 1개를 처리한다. */
	virtual void ProcessStep(int32 Index) = 0;

	/** 게임 스레드: 누적 결과를 JSON으로 저장한다. 성공 여부 반환. */
	virtual bool Finalize() = 0;

	/** "자세히" 로그용: Index번째 항목의 표시 이름(경로 등). 기본은 빈 문자열. */
	virtual FString GetStepLabel(int32 Index) const { return FString(); }
};

/**
 * 기존 동기 Export 함수(bool(FString&))를 단일 스텝 태스크로 감싸는 어댑터.
 *
 * 아직 per-asset 스텝으로 변환되지 않은 리포트를 러너에 그대로 태우기 위한 전환용.
 * ProcessStep(0)에서 리포트 전체를 한 번에 실행하므로 그 동안은 블로킹된다
 * (리포트 사이 정지/GC는 러너가 보장).
 */
class FLegacyReportTask : public IVaultReportTask
{
public:
	FLegacyReportTask(const FText& InLabel, bool (*InFunc)(FString&))
		: Label(InLabel), Func(InFunc) {}

	virtual FText GetLabel() const override { return Label; }
	virtual void Prepare(const FString& InOutputDir) override { OutputDir = InOutputDir; }
	virtual int32 NumSteps() const override { return 1; }
	virtual void ProcessStep(int32 /*Index*/) override { bResult = Func ? Func(OutputDir) : false; }
	virtual bool Finalize() override { return bResult; }

private:
	FText Label;
	bool (*Func)(FString&) = nullptr;
	FString OutputDir;
	bool bResult = false;
};
