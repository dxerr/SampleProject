// Copyright Epic Games, Inc. All Rights Reserved.

#include "VaultAssetCheckToolXlsxUtils.h"

#include "VaultAssetCheckToolReportUtils.h"

#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"
#include "Containers/Ticker.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

namespace VaultAssetCheckToolReport
{
#if ENGINE_MAJOR_VERSION >= 5
    // UE5 renamed FTicker to the thread-safe FTSTicker; its handle is a nested type.
    using FAppTicker = FTSTicker;
    using FAppTickerHandle = FTSTicker::FDelegateHandle;
#else
    // UE 4.27: FTicker with the global FDelegateHandle.
    using FAppTicker = FTicker;
    using FAppTickerHandle = FDelegateHandle;
#endif

    namespace
    {
        constexpr double kDebounceSeconds = 1.0;

        double LastRequestSeconds = 0.0;
        FString PendingOutputDir;
        FAppTickerHandle TickerHandle;

        bool LaunchJsonToXlsxExe(const FString& OutputDir)
        {
            const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("VaultAssetCheckTool"));
            if (!Plugin.IsValid())
            {
                UE_LOG(LogTemp, Warning, TEXT("VaultAssetCheckTool plugin not found. Skipping xlsx generation."));
                return false;
            }

            const FString ExePath = FPaths::Combine(
                Plugin->GetBaseDir(),
                TEXT("Resources"),
                TEXT("json_to_xlsx.exe"));

            if (!FPaths::FileExists(ExePath))
            {
                UE_LOG(LogTemp, Warning, TEXT("json_to_xlsx.exe not found at %s"), *ExePath);
                return false;
            }

            IFileManager& FileManager = IFileManager::Get();
            FileManager.MakeDirectory(*OutputDir, true);

            const FString ExeCopyPath = FPaths::Combine(OutputDir, TEXT("json_to_xlsx.exe"));
            if (FileManager.Copy(*ExeCopyPath, *ExePath, true, true) != COPY_OK)
            {
                UE_LOG(LogTemp, Warning, TEXT("json_to_xlsx.exe 복사 실패: %s -> %s"), *ExePath, *ExeCopyPath);
            }
            else
            {
                UE_LOG(LogTemp, Log, TEXT("json_to_xlsx.exe 복사: %s"), *ExeCopyPath);
            }

            const FString TemplatePath = FPaths::Combine(
                Plugin->GetBaseDir(),
                TEXT("Resources"),
                TEXT("summary.json"));

            FString Args = FString::Printf(TEXT("\"%s\""), *OutputDir);
            if (FPaths::FileExists(TemplatePath))
            {
                Args += FString::Printf(TEXT(" --template \"%s\""), *TemplatePath);
            }

            UE_LOG(LogTemp, Log, TEXT("json_to_xlsx.exe 실행: exe=%s, args=%s"), *ExePath, *Args);

            FProcHandle Handle = FPlatformProcess::CreateProc(
                *ExePath,
                *Args,
                true,
                false,
                false,
                nullptr,
                0,
                *FPaths::GetPath(ExePath),
                nullptr);

            if (!Handle.IsValid())
            {
                UE_LOG(LogTemp, Warning, TEXT("json_to_xlsx.exe 실행 실패: %s"), *ExePath);
                return false;
            }

            FPlatformProcess::CloseProc(Handle);
            return true;
        }

        bool TickPendingXlsx(float DeltaTime)
        {
            const double NowSeconds = FPlatformTime::Seconds();
            if (NowSeconds - LastRequestSeconds < kDebounceSeconds)
            {
                return true;
            }

            const FString OutputDir = PendingOutputDir;
            PendingOutputDir.Reset();

            UE_LOG(LogTemp, Log, TEXT("json_to_xlsx 예약 실행 시작: %s"), *OutputDir);
            LaunchJsonToXlsxExe(OutputDir);

            FAppTicker::GetCoreTicker().RemoveTicker(TickerHandle);
            TickerHandle.Reset();
            return false;
        }
    }

    bool RunJsonToXlsxExe(const FString& OutputPath)
    {
#if WITH_EDITOR
        const FString OutputDir = FPaths::ConvertRelativePathToFull(ResolveBaseDir(OutputPath));
        PendingOutputDir = OutputDir;
        LastRequestSeconds = FPlatformTime::Seconds();

        UE_LOG(LogTemp, Log, TEXT("json_to_xlsx 예약: outputDir=%s"), *OutputDir);

        if (!TickerHandle.IsValid())
        {
            TickerHandle = FAppTicker::GetCoreTicker().AddTicker(
                FTickerDelegate::CreateStatic(&TickPendingXlsx),
                0.1f);
        }

        return true;
#else
        return false;
#endif
    }
}
