// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/VaultAssetCheckToolSingleTableTask.h"
#include "Editor/VaultAssetCheckToolReportFactory.h"
#include "VaultAssetCheckTool.h" // ENGINE_MAJOR_VERSION
#include "VaultAssetCheckToolReportUtils.h"
#include "VaultAssetCheckToolXlsxUtils.h"

using namespace VaultAssetCheckToolReport;

bool VaultAssetCheckToolReport::RunTaskSynchronously(const TSharedPtr<IVaultReportTask>& Task, const FString& OutputPath)
{
	if (!Task.IsValid())
	{
		return false;
	}
	Task->Prepare(OutputPath);
	const int32 NumSteps = Task->NumSteps();
	for (int32 Index = 0; Index < NumSteps; ++Index)
	{
		Task->ProcessStep(Index);
	}
	return Task->Finalize();
}

void FSingleTableReportTask::AddClassToFilter(FARFilter& Filter, UClass* Class)
{
#if ENGINE_MAJOR_VERSION >= 5
	Filter.ClassPaths.Add(Class->GetClassPathName());
#else
	Filter.ClassNames.Add(Class->GetFName());
#endif
}

FString FSingleTableReportTask::GetObjectPathString(const FAssetData& AssetData)
{
#if ENGINE_MAJOR_VERSION >= 5
	return AssetData.GetObjectPathString();
#else
	return AssetData.ObjectPath.ToString();
#endif
}

void FSingleTableReportTask::Prepare(const FString& InOutputDir)
{
	OutputDir = InOutputDir;
	Assets.Reset();
	Rows.Reset();

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	GatherAssets(AssetRegistryModule.Get(), Assets);

	// 출력 순서 결정화: 경로순 정렬 → 실행마다 행 순서가 동일해 diff 비교가 깔끔해진다.
	Assets.Sort([](const FAssetData& A, const FAssetData& B)
	{
		return GetObjectPathString(A).Compare(GetObjectPathString(B), ESearchCase::IgnoreCase) < 0;
	});
}

void FSingleTableReportTask::ProcessStep(int32 Index)
{
	if (!Assets.IsValidIndex(Index))
	{
		return;
	}

	const FAssetData& AssetData = Assets[Index];
	if (!AssetData.PackageName.ToString().StartsWith(TEXT("/Game")))
	{
		return;
	}

	TSharedPtr<FJsonObject> Row = ExtractAsset(AssetData);
	if (Row.IsValid())
	{
		Rows.Add(MakeShareable(new FJsonValueObject(Row)));
	}
}

bool FSingleTableReportTask::Finalize()
{
	TArray<TSharedPtr<FJsonValue>> Notices = MakeNoticeList();
	AddReportNotices(Notices); // 서브클래스가 가이드 Notice를 채울 기회(기본 no-op)
	TSharedPtr<FJsonObject> RootObject = BuildSingleTableReport(
		Category,
		TableName,
		FString::Printf(TEXT("%s :%d"), *SummaryLabel, Rows.Num()),
		Rows,
		&Notices);

	const bool bSaved = SaveReportJson(OutputDir, Workbook, Sheet, RootObject);
	if (bSaved)
	{
		RunJsonToXlsxExe(OutputDir);
	}
	return bSaved;
}
