// Copyright Epic Games, Inc. All Rights Reserved.

#include "VaultAssetCheckTool.h"
#include "Editor/VaultAssetCheckToolSingleTableTask.h"
#include "Editor/VaultAssetCheckToolReportFactory.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimBoneCompressionSettings.h"
#include "Animation/Skeleton.h"

/** Animation 리포트 태스크: AnimSequence 1개당 1행. */
class FAnimationReportTask : public FSingleTableReportTask
{
public:
	FAnimationReportTask()
		: FSingleTableReportTask(
			NSLOCTEXT("VaultAssetCheckTool", "AnimationTaskLabel", "Animation"),
			TEXT("ResourceInfo"), TEXT("Animation"),
			TEXT("Animation List"), TEXT("Animation List"), TEXT("Total AnimationClip"))
	{}

protected:
	virtual void GatherAssets(IAssetRegistry& AssetRegistry, TArray<FAssetData>& OutAssets) override
	{
		FARFilter Filter;
		AddClassToFilter(Filter, UAnimSequence::StaticClass());
		Filter.PackagePaths.Add(FName("/Game"));
		Filter.bRecursivePaths = true;
		AssetRegistry.GetAssets(Filter, OutAssets);
	}

	virtual TSharedPtr<FJsonObject> ExtractAsset(const FAssetData& AssetData) override
	{
		UAnimSequence* AnimSequence = Cast<UAnimSequence>(AssetData.GetAsset());
		if (!AnimSequence)
		{
			return nullptr;
		}

		const FString AssetPath = GetObjectPathString(AssetData);
		const FString AssetName = AssetData.AssetName.ToString();

		const float Duration = AnimSequence->GetPlayLength();
		int32 NumberOfKeys = 0;
		int32 BoneTrackCount = 0;
#if ENGINE_MAJOR_VERSION >= 5
		const int32 FrameCount = AnimSequence->GetNumberOfSampledKeys();
		const float FrameRate = AnimSequence->GetSamplingFrameRate().AsDecimal();

		if (const IAnimationDataModel* DataModel = AnimSequence->GetDataModel())
		{
			BoneTrackCount = DataModel->GetNumBoneTracks();
			for (const FBoneAnimationTrack& Track : DataModel->GetBoneAnimationTracks())
			{
				NumberOfKeys += Track.InternalTrackData.PosKeys.Num();
				NumberOfKeys += Track.InternalTrackData.RotKeys.Num();
				NumberOfKeys += Track.InternalTrackData.ScaleKeys.Num();
			}
		}
#else
		const int32 FrameCount = AnimSequence->GetNumberOfFrames();
		const float FrameRate = AnimSequence->GetFrameRate();

		BoneTrackCount = AnimSequence->GetRawAnimationData().Num();
		for (const FRawAnimSequenceTrack& Track : AnimSequence->GetRawAnimationData())
		{
			NumberOfKeys += Track.PosKeys.Num();
			NumberOfKeys += Track.RotKeys.Num();
			NumberOfKeys += Track.ScaleKeys.Num();
		}
#endif

		int32 CurveCount = 0;
		if (const FRawCurveTracks* RawCurveData = &AnimSequence->GetCurveData())
		{
			CurveCount = RawCurveData->FloatCurves.Num();
		}

		const int32 NotifyCount = AnimSequence->Notifies.Num();
		const bool bHasRootMotion = AnimSequence->bEnableRootMotion;
		const bool bIsAdditive = AnimSequence->IsValidAdditive();
		const bool bIsCompressed = AnimSequence->IsCompressedDataValid();
		const bool bIsFullKey = (BoneTrackCount > 0) && (NumberOfKeys >= FrameCount * BoneTrackCount * 3);

		float KeyRatio = 0.0f;
		const int32 KeyDenominator = FrameCount * BoneTrackCount * 3;
		if (KeyDenominator != 0)
		{
			KeyRatio = FMath::RoundToFloat(static_cast<float>(NumberOfKeys) / KeyDenominator * 1000000.0f) / 1000000.0f;
		}

		FString BoneCompressionSettingsName = TEXT("None");
		if (AnimSequence->BoneCompressionSettings)
		{
			BoneCompressionSettingsName = AnimSequence->BoneCompressionSettings->GetName();
		}

		FString SkeletonName = TEXT("None");
		if (AnimSequence->GetSkeleton())
		{
			SkeletonName = AnimSequence->GetSkeleton()->GetName();
		}

		TSharedPtr<FJsonObject> AnimObject = MakeShareable(new FJsonObject);
		AnimObject->SetStringField(TEXT("assetPath"), AssetPath);
		AnimObject->SetStringField(TEXT("assetName"), AssetName);
		AnimObject->SetNumberField(TEXT("duration"), Duration);
		AnimObject->SetNumberField(TEXT("frameRate"), FrameRate);
		AnimObject->SetNumberField(TEXT("frameCount"), FrameCount);
		AnimObject->SetNumberField(TEXT("numberOfKeys"), NumberOfKeys);
		AnimObject->SetNumberField(TEXT("keyRatio"), KeyRatio);
		AnimObject->SetStringField(TEXT("boneCompressionSettingsName"), BoneCompressionSettingsName);
		AnimObject->SetBoolField(TEXT("isFullKey"), bIsFullKey);
		AnimObject->SetNumberField(TEXT("boneTrackCount"), BoneTrackCount);
		AnimObject->SetNumberField(TEXT("curveCount"), CurveCount);
		AnimObject->SetNumberField(TEXT("notifyCount"), NotifyCount);
		AnimObject->SetBoolField(TEXT("hasRootMotion"), bHasRootMotion);
		AnimObject->SetBoolField(TEXT("isAdditive"), bIsAdditive);
		AnimObject->SetBoolField(TEXT("isCompressed"), bIsCompressed);
		AnimObject->SetStringField(TEXT("skeleton"), SkeletonName);
		return AnimObject;
	}
};

TSharedPtr<IVaultReportTask> VaultAssetCheckToolReport::MakeAnimationTask()
{
	return MakeShared<FAnimationReportTask>();
}

bool FVaultAssetCheckToolModule::ExportAnimationReport(FString& OutputPath)
{
	return VaultAssetCheckToolReport::RunTaskSynchronously(VaultAssetCheckToolReport::MakeAnimationTask(), OutputPath);
}
