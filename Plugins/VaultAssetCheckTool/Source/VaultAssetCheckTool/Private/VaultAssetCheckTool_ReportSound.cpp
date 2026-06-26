// Copyright Epic Games, Inc. All Rights Reserved.

#include "VaultAssetCheckTool.h"
#include "VaultAssetCheckToolReportUtils.h"
#include "VaultAssetCheckToolXlsxUtils.h"
#include "Editor/VaultAssetCheckToolSingleTableTask.h"
#include "Editor/VaultAssetCheckToolReportFactory.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWave.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"

using namespace VaultAssetCheckToolReport;

/**
 * Sound 리포트 태스크: SoundWave 1개당 1행.
 * per-asset 스텝이므로 러너가 청크 처리 + 메모리 가드 + 즉시 정지를 적용할 수 있다.
 */
class FSoundReportTask : public FSingleTableReportTask
{
public:
	FSoundReportTask()
		: FSingleTableReportTask(
			NSLOCTEXT("VaultAssetCheckTool", "SoundTaskLabel", "Sound"),
			TEXT("ResourceInfo"), TEXT("Audio"),
			TEXT("Audio List"), TEXT("Audio List"), TEXT("Total Audio Clip"))
	{}

protected:
	virtual void GatherAssets(IAssetRegistry& AssetRegistry, TArray<FAssetData>& OutAssets) override
	{
		FARFilter Filter;
		AddClassToFilter(Filter, USoundWave::StaticClass());
		Filter.PackagePaths.Add(FName("/Game"));
		Filter.bRecursivePaths = true;
		AssetRegistry.GetAssets(Filter, OutAssets);
	}

	virtual TSharedPtr<FJsonObject> ExtractAsset(const FAssetData& AssetData) override
	{
		USoundWave* SoundWave = Cast<USoundWave>(AssetData.GetAsset());
		if (!SoundWave)
		{
			return nullptr;
		}

		const FString AssetPath = GetObjectPathString(AssetData);
		const FString AssetName = AssetData.AssetName.ToString();

		const float Duration = SoundWave->Duration;
		const int32 SampleRate = SoundWave->GetSampleRateForCurrentPlatform();
		const int32 NumChannels = SoundWave->NumChannels;
		const bool bIsLooping = SoundWave->bLooping;
		const bool bIsStreaming = SoundWave->IsStreaming();

		FString Format = TEXT("Unknown");
		TArray<uint8> OutRawPCMData;
		uint32 OutSampleRate = 0;
		uint16 OutNumChannels = 0;
		if (SoundWave->GetImportedSoundWaveData(OutRawPCMData, OutSampleRate, OutNumChannels))
		{
			if (OutRawPCMData.Num() > 4)
			{
				const uint8* Data = OutRawPCMData.GetData();
				if (Data[0] == 'R' && Data[1] == 'I' && Data[2] == 'F' && Data[3] == 'F')
					Format = TEXT("WAV");
				else if (Data[0] == 'O' && Data[1] == 'g' && Data[2] == 'g' && Data[3] == 'S')
					Format = TEXT("OGG");
				else if ((Data[0] == 0xFF && (Data[1] & 0xE0) == 0xE0) ||
					(Data[0] == 'I' && Data[1] == 'D' && Data[2] == '3'))
					Format = TEXT("MP3");
				else
					Format = TEXT("PCM");
			}
		}

		// SizeMB는 임포트 원본 포맷(Format)이 아니라, 플랫폼이 실제로 쿡하는
		// 런타임 코덱 포맷명으로 압축 데이터 크기를 조회해야 한다.
		float SizeMB = 0.0f;
		FName RuntimeFormat = NAME_None;
		if (ITargetPlatformManagerModule* TPM = GetTargetPlatformManager())
		{
			if (const ITargetPlatform* RunningPlatform = TPM->GetRunningTargetPlatform())
			{
				RuntimeFormat = RunningPlatform->GetWaveFormat(SoundWave);
			}
		}
		if (RuntimeFormat != NAME_None)
		{
			const int32 CompressedSize = SoundWave->GetCompressedDataSize(RuntimeFormat);
			SizeMB = CompressedSize / (1024.0f * 1024.0f);
		}

		// 초당 평균 데이터량. SizeMB 또는 Duration이 0이면 0.0.
		const float MbPerSec = (SizeMB > 0.0f && Duration > 0.0f) ? (SizeMB / Duration) : 0.0f;

		FString CompressionQuality = TEXT("Default");
#if ENGINE_MAJOR_VERSION >= 5
		const int32 CompressionQualityValue = SoundWave->GetCompressionQuality();
#else
		const int32 CompressionQualityValue = SoundWave->CompressionQuality;
#endif
		switch (CompressionQualityValue)
		{
		case 1: CompressionQuality = TEXT("Lowest"); break;
		case 10: CompressionQuality = TEXT("Low"); break;
		case 25: CompressionQuality = TEXT("Medium-Low"); break;
		case 50: CompressionQuality = TEXT("Medium"); break;
		case 75: CompressionQuality = TEXT("High"); break;
		case 100: CompressionQuality = TEXT("Highest"); break;
		default: CompressionQuality = FString::Printf(TEXT("%d"), CompressionQualityValue); break;
		}

		const float Volume = SoundWave->Volume;
		const float Pitch = SoundWave->Pitch;

		FString AttenuationInfo = TEXT("None");
		if (SoundWave->AttenuationSettings)
		{
			AttenuationInfo = SoundWave->AttenuationSettings->GetName();
		}

		const bool bIsProcedural = SoundWave->bProcedural;
		const bool bIsMature = SoundWave->bMature;

		TSharedPtr<FJsonObject> SoundObject = MakeShareable(new FJsonObject);
		SoundObject->SetStringField(TEXT("assetPath"), AssetPath);
		SoundObject->SetStringField(TEXT("assetName"), AssetName);
		SoundObject->SetNumberField(TEXT("duration"), Duration);
		SoundObject->SetNumberField(TEXT("sampleRate"), SampleRate);
		SoundObject->SetNumberField(TEXT("channels"), NumChannels);
		SoundObject->SetBoolField(TEXT("isLooping"), bIsLooping);
		SoundObject->SetBoolField(TEXT("isStreaming"), bIsStreaming);
		SoundObject->SetStringField(TEXT("compressionQuality"), CompressionQuality);
		SoundObject->SetNumberField(TEXT("sizeMB"), SizeMB);
		SoundObject->SetNumberField(TEXT("mbPerSec"), MbPerSec);
		SoundObject->SetStringField(TEXT("format"), Format);
		SoundObject->SetNumberField(TEXT("volume"), Volume);
		SoundObject->SetNumberField(TEXT("pitch"), Pitch);
		SoundObject->SetStringField(TEXT("attenuation"), AttenuationInfo);
		SoundObject->SetBoolField(TEXT("isProcedural"), bIsProcedural);
		SoundObject->SetBoolField(TEXT("isMature"), bIsMature);

		return SoundObject;
	}
};

TSharedPtr<IVaultReportTask> VaultAssetCheckToolReport::MakeSoundTask()
{
	return MakeShared<FSoundReportTask>();
}

// 동기 경로(블루프린트 라이브러리/EUW 호환): 동일 태스크를 그 자리에서 끝까지 실행한다.
bool FVaultAssetCheckToolModule::ExportSoundReport(FString& OutputPath)
{
	return VaultAssetCheckToolReport::RunTaskSynchronously(VaultAssetCheckToolReport::MakeSoundTask(), OutputPath);
}
