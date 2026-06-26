// Fill out your copyright notice in the Description page of Project Settings.

#include "VaultAssetCheckToolLibrary.h"
#include "VaultAssetCheckTool.h"

void UVaultAssetCheckToolLibrary::ExportTextureReport(FString OutputPath)
{
    FVaultAssetCheckToolModule::ExportTextureReport(OutputPath);
}

void UVaultAssetCheckToolLibrary::ExportSkeletalMeshReport(FString OutputPath)
{
    FVaultAssetCheckToolModule::ExportSkeletalMeshReport(OutputPath);
}

void UVaultAssetCheckToolLibrary::ExportStaticMeshReport(FString OutputPath)
{
    FVaultAssetCheckToolModule::ExportStaticMeshReport(OutputPath);
}

void UVaultAssetCheckToolLibrary::ExportUMGAssetReport(FString OutputPath)
{
	FVaultAssetCheckToolModule::ExportUMGAssetReport(OutputPath);
}

void UVaultAssetCheckToolLibrary::ExportAnimationReport(FString OutputPath)
{
	FVaultAssetCheckToolModule::ExportAnimationReport(OutputPath);
}

void UVaultAssetCheckToolLibrary::ExportSoundReport(FString OutputPath)
{
	FVaultAssetCheckToolModule::ExportSoundReport(OutputPath);
}

void UVaultAssetCheckToolLibrary::ExportFontReport(FString OutputPath)
{
	FVaultAssetCheckToolModule::ExportFontReport(OutputPath);
}

void UVaultAssetCheckToolLibrary::ExportNiagaraReport(FString OutputPath)
{
	FVaultAssetCheckToolModule::ExportNiagaraReport(OutputPath);
}

void UVaultAssetCheckToolLibrary::ExportCascadeReport(FString OutputPath)
{
	FVaultAssetCheckToolModule::ExportCascadeReport(OutputPath);
}

void UVaultAssetCheckToolLibrary::ExportProjectInfoReport(FString OutputPath)
{
	FVaultAssetCheckToolModule::ExportProjectInfoReport(OutputPath);
}

void UVaultAssetCheckToolLibrary::OpenFolderInExplorer(FString FolderPath)
{
	// 절대 경로로 변환
	FString AbsolutePath = FPaths::ConvertRelativePathToFull(FolderPath);

	// 폴더가 존재하는지 확인
	if (FPaths::DirectoryExists(AbsolutePath))
	{
		FPlatformProcess::ExploreFolder(*AbsolutePath);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Folder does not exist: %s"), *AbsolutePath);
	}
}
