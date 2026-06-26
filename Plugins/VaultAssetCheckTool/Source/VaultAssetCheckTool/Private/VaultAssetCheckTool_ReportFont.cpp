// Copyright Epic Games, Inc. All Rights Reserved.

#include "VaultAssetCheckTool.h"
#include "Editor/VaultAssetCheckToolSingleTableTask.h"
#include "Editor/VaultAssetCheckToolReportFactory.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Engine/Texture2D.h"

#include "VaultAssetCheckToolFontUtils.h"

/** Font 리포트 태스크: UFont 1개당 1행. */
class FFontReportTask : public FSingleTableReportTask
{
public:
	FFontReportTask()
		: FSingleTableReportTask(
			NSLOCTEXT("VaultAssetCheckTool", "FontTaskLabel", "Font"),
			TEXT("ResourceInfo"), TEXT("Font"),
			TEXT("Font List"), TEXT("Font List"), TEXT("Total Font"))
	{}

protected:
	virtual void GatherAssets(IAssetRegistry& AssetRegistry, TArray<FAssetData>& OutAssets) override
	{
		FARFilter Filter;
		AddClassToFilter(Filter, UFont::StaticClass());
		Filter.PackagePaths.Add(FName("/Game"));
		Filter.bRecursivePaths = true;
		AssetRegistry.GetAssets(Filter, OutAssets);
	}

	virtual TSharedPtr<FJsonObject> ExtractAsset(const FAssetData& AssetData) override
	{
		UFont* Font = Cast<UFont>(AssetData.GetAsset());
		if (!Font)
		{
			return nullptr;
		}

		TSharedPtr<FJsonObject> FontObject = MakeShareable(new FJsonObject);
		FontObject->SetStringField(TEXT("assetPath"), GetObjectPathString(AssetData));
		FontObject->SetStringField(TEXT("assetName"), AssetData.AssetName.ToString());
		FontObject->SetNumberField(TEXT("ascent"), Font->Ascent);
		FontObject->SetNumberField(TEXT("descent"), Font->Descent);
		FontObject->SetNumberField(TEXT("leading"), Font->Leading);
		FontObject->SetStringField(TEXT("cacheType"), Font->FontCacheType == EFontCacheType::Runtime ? TEXT("Runtime") : TEXT("Offline"));

		double TotalSizeByte = 0.0;
		TArray<TSharedPtr<FJsonValue>> FacesArray;

		if (Font->FontCacheType == EFontCacheType::Runtime)
		{
			for (const FTypefaceEntry& Entry : Font->CompositeFont.DefaultTypeface.Fonts)
			{
				TSharedPtr<FJsonObject> FaceObject = MakeShareable(new FJsonObject);
				FaceObject->SetStringField(TEXT("faceName"), Entry.Name.ToString());

				if (const UFontFace* FontFace = Cast<UFontFace>(Entry.Font.GetFontFaceAsset()))
				{
					FaceObject->SetStringField(TEXT("fontFaceAsset"), FontFace->GetPathName());
					FaceObject->SetStringField(TEXT("fontFilename"), FontFace->GetFontFilename());
					FaceObject->SetStringField(TEXT("loadingPolicy"), UEnum::GetValueAsString(FontFace->GetLoadingPolicy()));

#if ENGINE_MAJOR_VERSION >= 5
					TSharedPtr<const FFontFaceData> FaceData = FontFace->GetFontFaceData();
					if (FaceData.IsValid())
					{
						const double FaceSize = (double)FaceData->GetData().Num();
						FaceObject->SetNumberField(TEXT("sizeKB"), FaceSize / 1024.0);
						TotalSizeByte += FaceSize;

						VaultAssetCheckToolReport::ExtractGlyphCoverage(FaceData->GetData(), FaceObject);
					}
#else
					FFontFaceDataConstRef FaceData = FontFace->GetFontFaceData();
					{
						const double FaceSize = (double)FaceData->GetData().Num();
						FaceObject->SetNumberField(TEXT("sizeKB"), FaceSize / 1024.0);
						TotalSizeByte += FaceSize;

						VaultAssetCheckToolReport::ExtractGlyphCoverage(FaceData->GetData(), FaceObject);
					}
#endif
				}
				FacesArray.Add(MakeShareable(new FJsonValueObject(FaceObject)));
			}

			TArray<TSharedPtr<FJsonValue>> RangesArray;
			for (const FCompositeSubFont& SubFont : Font->CompositeFont.SubTypefaces)
			{
				for (const FInt32Range& Range : SubFont.CharacterRanges)
				{
					RangesArray.Add(MakeShareable(new FJsonValueString(
						FString::Printf(TEXT("U+%04X - U+%04X"), Range.GetLowerBoundValue(), Range.GetUpperBoundValue()))));
				}
			}
			FontObject->SetArrayField(TEXT("supportedRanges"), RangesArray);
		}
		else
		{
			FontObject->SetNumberField(TEXT("glyphCount"), Font->Characters.Num());

			FResourceSizeEx TotalResSize;
			for (UTexture2D* FontTexture : Font->Textures)
			{
				if (FontTexture)
				{
					FontTexture->GetResourceSizeEx(TotalResSize);
				}
			}
			TotalSizeByte = (double)TotalResSize.GetTotalMemoryBytes();
		}

		FontObject->SetArrayField(TEXT("faces"), FacesArray);
		FontObject->SetNumberField(TEXT("totalSizeKB"), TotalSizeByte / 1024.0);
		return FontObject;
	}
};

TSharedPtr<IVaultReportTask> VaultAssetCheckToolReport::MakeFontTask()
{
	return MakeShared<FFontReportTask>();
}

bool FVaultAssetCheckToolModule::ExportFontReport(FString& OutputPath)
{
	return VaultAssetCheckToolReport::RunTaskSynchronously(VaultAssetCheckToolReport::MakeFontTask(), OutputPath);
}
