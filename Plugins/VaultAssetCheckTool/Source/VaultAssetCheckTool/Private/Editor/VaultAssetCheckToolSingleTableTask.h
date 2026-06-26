// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Editor/VaultAssetCheckToolReportTask.h"
#include "AssetRegistry/AssetRegistryModule.h" // FAssetData, FARFilter, IAssetRegistry
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"

/**
 * 단일 테이블(에셋 1종 → 행 1개) 리포트용 per-asset 스텝 태스크 베이스.
 *
 * 서브클래스는 GatherAssets()(대상 필터)와 ExtractAsset()(에셋 1개 → JSON 행)만 구현한다.
 * 수집/스텝/저장 흐름과 /Game 필터는 베이스가 처리한다.
 */
class FSingleTableReportTask : public IVaultReportTask
{
public:
	FSingleTableReportTask(
		const FText& InLabel,
		const FString& InWorkbook,
		const FString& InSheet,
		const FString& InCategory,
		const FString& InTableName,
		const FString& InSummaryLabel)
		: Label(InLabel)
		, Workbook(InWorkbook)
		, Sheet(InSheet)
		, Category(InCategory)
		, TableName(InTableName)
		, SummaryLabel(InSummaryLabel)
	{}

	virtual FText GetLabel() const override { return Label; }
	virtual void Prepare(const FString& InOutputDir) override;
	virtual int32 NumSteps() const override { return Assets.Num(); }
	virtual void ProcessStep(int32 Index) override;
	virtual bool Finalize() override;
	virtual FString GetStepLabel(int32 Index) const override
	{
		return Assets.IsValidIndex(Index) ? GetObjectPathString(Assets[Index]) : FString();
	}

protected:
	/** 대상 에셋을 수집한다(클래스 필터 등). */
	virtual void GatherAssets(IAssetRegistry& AssetRegistry, TArray<FAssetData>& OutAssets) = 0;

	/** 에셋 1개를 로드/분석해 JSON 행을 만든다. 건너뛸 경우 nullptr 반환. */
	virtual TSharedPtr<FJsonObject> ExtractAsset(const FAssetData& AssetData) = 0;

	/** 리포트 상단에 표기할 가이드 Notice를 추가한다(선택). 기본은 없음. */
	virtual void AddReportNotices(TArray<TSharedPtr<FJsonValue>>& Notices) const {}

	// UE4/UE5 API 차이를 흡수하는 공용 헬퍼
	static void AddClassToFilter(FARFilter& Filter, UClass* Class);
	static FString GetObjectPathString(const FAssetData& AssetData);

	FText Label;
	FString Workbook;
	FString Sheet;
	FString Category;
	FString TableName;
	FString SummaryLabel;

	FString OutputDir;
	TArray<FAssetData> Assets;
	TArray<TSharedPtr<FJsonValue>> Rows;
};
