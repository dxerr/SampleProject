// Copyright Epic Games, Inc. All Rights Reserved.

#include "VaultAssetCheckTool.h"
#include "VaultAssetCheckToolReportUtils.h"
#include "VaultAssetCheckToolXlsxUtils.h"

#include "BuildSettings.h"
#include "Interfaces/IPluginManager.h"
#include "Settings/ProjectPackagingSettings.h"
#include "Engine/UserInterfaceSettings.h"
#include "AndroidRuntimeSettings.h"
#include "IOSRuntimeSettings.h"
#include "WindowsTargetSettings.h"

using namespace VaultAssetCheckToolReport;

bool FVaultAssetCheckToolModule::ExportProjectInfoReport(FString& OutputPath)
{
    // Notice dictionary (extend here)
    const TMap<FString, FString> ProjectNoticeByItem = {
        //{TEXT("Engine Version"), TEXT("프로젝트 엔진 버전 확인")}
    };
    const TMap<FString, FString> PluginNoticeByItem = {
        //{TEXT("Enabled Plugins"), TEXT("활성 플러그인 목록 확인")}
    };

    TSharedPtr<FJsonObject> ProjectObject = MakeShareable(new FJsonObject);
    ProjectObject->SetStringField(TEXT("name"), FApp::GetProjectName());

    FEngineVersion EngineVer = FEngineVersion::Current();
    TSharedPtr<FJsonObject> EngineObject = MakeShareable(new FJsonObject);
    EngineObject->SetStringField(TEXT("version"), FString::Printf(TEXT("%d.%d.%d-%d"), EngineVer.GetMajor(), EngineVer.GetMinor(), EngineVer.GetPatch(), EngineVer.GetChangelist()));
    EngineObject->SetStringField(TEXT("branchName"), EngineVer.GetBranch());
    EngineObject->SetBoolField(TEXT("isCustomEngine"), !BuildSettings::IsPromotedBuild());
    ProjectObject->SetObjectField(TEXT("engine"), EngineObject);

    const UProjectPackagingSettings* PackagingSettings = GetDefault<UProjectPackagingSettings>();
    if (PackagingSettings)
    {
        TSharedPtr<FJsonObject> PackagingObject = MakeShareable(new FJsonObject);
        PackagingObject->SetBoolField(TEXT("usePakFile"), PackagingSettings->UsePakFile);
        PackagingObject->SetBoolField(TEXT("useIoStore"), PackagingSettings->bUseIoStore);
        PackagingObject->SetBoolField(TEXT("generateChunks"), PackagingSettings->bGenerateChunks);
        PackagingObject->SetNumberField(TEXT("maxChunkSize"), PackagingSettings->MaxChunkSize);
        ProjectObject->SetObjectField(TEXT("packaging"), PackagingObject);
    }

    const UUserInterfaceSettings* UISettings = GetDefault<UUserInterfaceSettings>();
    if (UISettings)
    {
        TSharedPtr<FJsonObject> UIObject = MakeShareable(new FJsonObject);
        UIObject->SetNumberField(TEXT("applicationScale"), UISettings->ApplicationScale);
        UIObject->SetStringField(TEXT("uiScaleRule"), StaticEnum<EUIScalingRule>()->GetNameStringByValue(static_cast<int64>(UISettings->UIScaleRule)));
        UIObject->SetStringField(TEXT("customScalingRuleClass"), UISettings->CustomScalingRuleClass.ToString());
        UIObject->SetBoolField(TEXT("allowHighDPIInGameMode"), UISettings->bAllowHighDPIInGameMode);

        TSharedPtr<FJsonObject> DesignSizeObject = MakeShareable(new FJsonObject);
        DesignSizeObject->SetNumberField(TEXT("x"), UISettings->DesignScreenSize.X);
        DesignSizeObject->SetNumberField(TEXT("y"), UISettings->DesignScreenSize.Y);
        UIObject->SetObjectField(TEXT("designScreenSize"), DesignSizeObject);

        ProjectObject->SetObjectField(TEXT("userInterface"), UIObject);
    }

    TSharedPtr<FJsonObject> PlatformObject = MakeShareable(new FJsonObject);

    const UAndroidRuntimeSettings* AndroidSettings = GetDefault<UAndroidRuntimeSettings>();
    if (AndroidSettings)
    {
        TSharedPtr<FJsonObject> AndroidObject = MakeShareable(new FJsonObject);
        AndroidObject->SetNumberField(TEXT("minSDKVersion"), AndroidSettings->MinSDKVersion);
        AndroidObject->SetNumberField(TEXT("targetSDKVersion"), AndroidSettings->TargetSDKVersion);
        AndroidObject->SetStringField(TEXT("orientation"), StaticEnum<EAndroidScreenOrientation::Type>()->GetNameStringByValue(static_cast<int64>(AndroidSettings->Orientation)));
#if ENGINE_MAJOR_VERSION >= 5
        AndroidObject->SetNumberField(TEXT("minAspectRatio"), AndroidSettings->MinAspectRatio);
#endif
        AndroidObject->SetNumberField(TEXT("maxAspectRatio"), AndroidSettings->MaxAspectRatio);
        AndroidObject->SetStringField(TEXT("depthBufferPreference"), StaticEnum<EAndroidDepthBufferPreference::Type>()->GetNameStringByValue(static_cast<int64>(AndroidSettings->DepthBufferPreference)));
        AndroidObject->SetBoolField(TEXT("buildForArm64"), AndroidSettings->bBuildForArm64);
        AndroidObject->SetBoolField(TEXT("buildForX8664"), AndroidSettings->bBuildForX8664);
        AndroidObject->SetBoolField(TEXT("buildForES31"), AndroidSettings->bBuildForES31);
        AndroidObject->SetBoolField(TEXT("supportsVulkan"), AndroidSettings->bSupportsVulkan);
        AndroidObject->SetBoolField(TEXT("supportsVulkanSM5"), AndroidSettings->bSupportsVulkanSM5);
#if ENGINE_MAJOR_VERSION >= 5
        AndroidObject->SetBoolField(TEXT("packageForMetaQuest"), AndroidSettings->bPackageForMetaQuest);
#endif
        AndroidObject->SetBoolField(TEXT("enableBundle"), AndroidSettings->bEnableBundle);
        PlatformObject->SetObjectField(TEXT("android"), AndroidObject);
    }

    const UIOSRuntimeSettings* IOSSettings = GetDefault<UIOSRuntimeSettings>();
    if (IOSSettings)
    {
        TSharedPtr<FJsonObject> IOSObject = MakeShareable(new FJsonObject);
        IOSObject->SetStringField(TEXT("minimumIOSVersion"), StaticEnum<EIOSVersion>()->GetNameStringByValue(static_cast<int64>(IOSSettings->MinimumiOSVersion)));
        IOSObject->SetBoolField(TEXT("supportsMetal"), IOSSettings->bSupportsMetal);
        // UE5.8: UIOSRuntimeSettings::bSupportsMetalMRT 제거됨 → 5.8 미만에서만 기록
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 8)
        IOSObject->SetBoolField(TEXT("supportsMetalMRT"), IOSSettings->bSupportsMetalMRT);
#endif
#if ENGINE_MAJOR_VERSION >= 5
        IOSObject->SetBoolField(TEXT("supportHighRefreshRates"), IOSSettings->bSupportHighRefreshRates);
#endif
        IOSObject->SetStringField(TEXT("frameRateLock"), StaticEnum<EPowerUsageFrameRateLock>()->GetNameStringByValue(static_cast<int64>(IOSSettings->FrameRateLock)));
        IOSObject->SetBoolField(TEXT("enableDynamicMaxFPS"), IOSSettings->bEnableDynamicMaxFPS);
#if ENGINE_MAJOR_VERSION >= 5
        IOSObject->SetNumberField(TEXT("metalLanguageVersion"), IOSSettings->MetalLanguageVersion);
        // UE5.8: UIOSRuntimeSettings::bSupportAppleA8 제거됨 → 5.8 미만에서만 기록
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 8)
        IOSObject->SetBoolField(TEXT("supportAppleA8"), IOSSettings->bSupportAppleA8);
#endif
#else
        // UE 4.27: equivalent member is MaxShaderLanguageVersion (uint8); no bSupportAppleA8.
        IOSObject->SetNumberField(TEXT("metalLanguageVersion"), IOSSettings->MaxShaderLanguageVersion);
#endif
        IOSObject->SetBoolField(TEXT("supportsIPad"), IOSSettings->bSupportsIPad);
        IOSObject->SetBoolField(TEXT("supportsIPhone"), IOSSettings->bSupportsIPhone);
        PlatformObject->SetObjectField(TEXT("ios"), IOSObject);
    }

    const UWindowsTargetSettings* WindowsSettings = GetDefault<UWindowsTargetSettings>();
    if (WindowsSettings)
    {
        TSharedPtr<FJsonObject> WindowsObject = MakeShareable(new FJsonObject);
        WindowsObject->SetStringField(TEXT("defaultGraphicsRHI"), StaticEnum<EDefaultGraphicsRHI>()->GetNameStringByValue(static_cast<int64>(WindowsSettings->DefaultGraphicsRHI)));

#if ENGINE_MAJOR_VERSION >= 5
        TArray<TSharedPtr<FJsonValue>> D3D12Formats;
        for (const FString& Format : WindowsSettings->D3D12TargetedShaderFormats)
        {
            D3D12Formats.Add(MakeShareable(new FJsonValueString(Format)));
        }
        WindowsObject->SetArrayField(TEXT("d3d12TargetedShaderFormats"), D3D12Formats);

        TArray<TSharedPtr<FJsonValue>> D3D11Formats;
        for (const FString& Format : WindowsSettings->D3D11TargetedShaderFormats)
        {
            D3D11Formats.Add(MakeShareable(new FJsonValueString(Format)));
        }
        WindowsObject->SetArrayField(TEXT("d3d11TargetedShaderFormats"), D3D11Formats);

        TArray<TSharedPtr<FJsonValue>> VulkanFormats;
        for (const FString& Format : WindowsSettings->VulkanTargetedShaderFormats)
        {
            VulkanFormats.Add(MakeShareable(new FJsonValueString(Format)));
        }
        WindowsObject->SetArrayField(TEXT("vulkanTargetedShaderFormats"), VulkanFormats);

        WindowsObject->SetBoolField(TEXT("generateNaniteFallbackMeshes"), WindowsSettings->bGenerateNaniteFallbackMeshes);
#else
        // UE 4.27: shader formats live in a single TargetedRHIs array (no per-API split),
        // and there are no Nanite settings.
        TArray<TSharedPtr<FJsonValue>> TargetedRHIList;
        for (const FString& Format : WindowsSettings->TargetedRHIs)
        {
            TargetedRHIList.Add(MakeShareable(new FJsonValueString(Format)));
        }
        WindowsObject->SetArrayField(TEXT("targetedRHIs"), TargetedRHIList);
#endif
        PlatformObject->SetObjectField(TEXT("windows"), WindowsObject);
    }

    if (PlatformObject->Values.Num() > 0)
    {
        ProjectObject->SetObjectField(TEXT("platformSettings"), PlatformObject);
    }

    int MaxUObjects = 0;
    GConfig->GetInt(TEXT("/Script/Engine.GarbageCollectionSettings"), TEXT("gc.MaxObjectsInGame"), MaxUObjects, GEngineIni);
    TSharedPtr<FJsonObject> GCObject = MakeShareable(new FJsonObject);
    GCObject->SetNumberField(TEXT("maxObjectsInGame"), MaxUObjects);
    ProjectObject->SetObjectField(TEXT("garbageCollection"), GCObject);

    TArray<TSharedPtr<FJsonValue>> ProjectRows;
    ProjectRows.Add(MakeShareable(new FJsonValueObject(ProjectObject)));

    TArray<TSharedPtr<FJsonValue>> ProjectNotices = MakeNoticeList();
    for (const TPair<FString, FString>& NoticePair : ProjectNoticeByItem)
    {
        AddNotice(ProjectNotices, NoticePair.Key, NoticePair.Value);
    }
    TSharedPtr<FJsonObject> ProjectRoot = BuildSingleTableReport(
        TEXT("[ProjectInfo]"),
        TEXT("[ProjectInfo]"),
        TEXT(""),
        ProjectRows,
        &ProjectNotices);

    const bool bProjectSaved = SaveReportJson(OutputPath, TEXT("ProjectInfo"), TEXT("ProjectInfo"), ProjectRoot);

    TArray<TSharedPtr<FJsonValue>> PluginsArray;
    TArray<TSharedRef<IPlugin>> EnabledPlugins = IPluginManager::Get().GetEnabledPlugins();
    for (const TSharedRef<IPlugin>& Plugin : EnabledPlugins)
    {
        TSharedPtr<FJsonObject> PluginObject = MakeShareable(new FJsonObject);
        PluginObject->SetStringField(TEXT("name"), Plugin->GetName());
        PluginObject->SetStringField(TEXT("version"), Plugin->GetDescriptor().VersionName);
        PluginObject->SetStringField(TEXT("category"), Plugin->GetDescriptor().Category);
        PluginsArray.Add(MakeShareable(new FJsonValueObject(PluginObject)));
    }

    TArray<TSharedPtr<FJsonValue>> PluginNotices = MakeNoticeList();
    for (const TPair<FString, FString>& NoticePair : PluginNoticeByItem)
    {
        AddNotice(PluginNotices, NoticePair.Key, NoticePair.Value);
    }
    TSharedPtr<FJsonObject> PluginsRoot = BuildSingleTableReport(
        TEXT("[Plugins]"),
        TEXT("[Plugins]"),
        FString::Printf(TEXT("Total Plugin :%d"), PluginsArray.Num()),
        PluginsArray,
        &PluginNotices);

    const bool bPluginsSaved = SaveReportJson(OutputPath, TEXT("ProjectInfo"), TEXT("Plugins"), PluginsRoot);

    const bool bSaved = bProjectSaved && bPluginsSaved;
    if (bSaved)
    {
        RunJsonToXlsxExe(OutputPath);
    }
    return bSaved;
}
