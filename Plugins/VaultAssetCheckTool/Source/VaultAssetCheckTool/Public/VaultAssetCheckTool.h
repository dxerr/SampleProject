// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "Misc/EngineVersionComparison.h" // ENGINE_MAJOR_VERSION (UE4/UE5 분기용)

class FVaultAssetCheckToolModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static bool ExportTextureReport(FString& OutputPath);
	static bool ExportSkeletalMeshReport(FString& OutputPath);
	static bool ExportStaticMeshReport(FString& OutputPath);
	static bool ExportUMGAssetReport(FString& OutputPath);
	static bool ExportAnimationReport(FString& OutputPath);
	static bool ExportSoundReport(FString& OutputPath);
	static bool ExportFontReport(FString& OutputPath);
	static bool ExportNiagaraReport(FString& OutputPath);
	static bool ExportCascadeReport(FString& OutputPath);
	static bool ExportProjectInfoReport(FString& OutputPath);
};
