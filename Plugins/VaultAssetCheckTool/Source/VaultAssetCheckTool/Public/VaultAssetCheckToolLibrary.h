// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VaultAssetCheckToolLibrary.generated.h"

UCLASS(transient)
class UVaultAssetCheckToolLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VaultAssetCheckTool")
	static void ExportTextureReport(FString OutputPath);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VaultAssetCheckTool")
	static void ExportSkeletalMeshReport(FString OutputPath);
	
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VaultAssetCheckTool")
	static void ExportStaticMeshReport(FString OutputPath);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VaultAssetCheckTool")
	static void ExportUMGAssetReport(FString OutputPath);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VaultAssetCheckTool")
	static void ExportAnimationReport(FString OutputPath);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VaultAssetCheckTool")
	static void ExportSoundReport(FString OutputPath);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VaultAssetCheckTool")
	static void ExportFontReport(FString OutputPath);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VaultAssetCheckTool")
	static void ExportNiagaraReport(FString OutputPath);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VaultAssetCheckTool")
	static void ExportCascadeReport(FString OutputPath);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VaultAssetCheckTool")
	static void ExportProjectInfoReport(FString OutputPath);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VaultAssetCheckTool")
	static void OpenFolderInExplorer(FString FolderPath);
};