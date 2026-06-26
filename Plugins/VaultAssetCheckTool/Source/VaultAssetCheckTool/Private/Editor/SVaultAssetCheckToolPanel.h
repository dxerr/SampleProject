// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Editor/VaultAssetCheckToolJobRunner.h"

class SEditableTextBox;
class SMultiLineEditableTextBox;
class IVaultReportTask;
enum class ECheckBoxState : uint8;

/**
 * VaultAssetCheckTool 리포트 추출 패널.
 *
 * 상/하 2분할: [상] 출력 경로 + 리포트 버튼([버튼+설명]) + 전체 출력 + 정지 + 진행바, [하] 스크롤 로그.
 * 실제 실행은 FVaultAssetCheckToolJobRunner가 게임 스레드 청크로 구동(메모리 가드 포함).
 */
class SVaultAssetCheckToolPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVaultAssetCheckToolPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** 리포트 1건 정의: 라벨 + 설명 + 동기 함수(BP/레거시) + per-asset 태스크 팩토리(옵션). */
	struct FReportEntry
	{
		FText Label;
		FText Description;
		bool (*Func)(FString&);
		TSharedPtr<IVaultReportTask> (*MakeTaskFn)(); // nullptr이면 레거시 어댑터로 폴백
	};

	/** 패널에 노출할 리포트 목록. (버튼 추가/제거는 여기만 수정) */
	static TArray<FReportEntry> GetReportEntries();

	static FString GetDefaultOutputDir();
	FString GetCurrentOutputPath() const;
	void EnsureOutputDir(const FString& Path) const;

	/** 엔트리 1건을 태스크로 만든다. (Stage B에서 변환된 리포트는 전용 태스크 반환) */
	static TSharedPtr<IVaultReportTask> MakeTask(const FReportEntry& Entry);

	void StartTasks(const TArray<TSharedPtr<IVaultReportTask>>& Tasks);

	// --- 버튼 핸들러 ---
	FReply OnRunSingle(int32 EntryIndex);
	FReply OnRunAll();
	FReply OnStop();
	FReply OnOpenOutputFolder();

	// --- 러너 콜백 ---
	void AppendLog(const FString& Line, bool bDetailOnly);
	void FlushLog(bool bForce);            // 활성 버퍼를 위젯에 반영(스로틀)
	void HandleProgress(float Percent, const FString& Phase);
	void HandleFinished(bool bInterrupted);

	// "자세히" 체크박스
	ECheckBoxState GetVerboseCheckState() const;
	void OnVerboseCheckChanged(ECheckBoxState NewState);

	// --- 위젯 상태 ---
	bool AreControlsEnabled() const;
	bool IsStopEnabled() const;
	TOptional<float> GetProgressPercent() const;

	TSharedPtr<SEditableTextBox> OutputPathBox;
	TSharedPtr<SMultiLineEditableTextBox> LogTextBox; // 읽기전용 + 선택/복사 가능
	FString SimpleLog;              // 간단 로그(요약/시작/완료/결과)
	FString DetailedLog;            // 상세 로그(간단 + 항목별) — 체크박스 무관하게 항상 기록
	bool bVerboseLog = false;       // "자세히" 체크 상태(표시 버퍼 선택)
	bool bLogDirty = false;         // 위젯 반영 대기 중인 로그 변경
	double LastLogFlushSeconds = 0.0;

	float ProgressPercent = 0.0f;

	FVaultAssetCheckToolJobRunner Runner;
};
