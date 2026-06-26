// Copyright Epic Games, Inc. All Rights Reserved.

#include "VaultAssetCheckTool.h"
#include "VaultAssetCheckToolReportUtils.h"
#include "VaultAssetCheckToolXlsxUtils.h"
#include "Editor/VaultAssetCheckToolReportTask.h"
#include "Editor/VaultAssetCheckToolReportFactory.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureCube.h"
#include "Engine/TextureRenderTarget2D.h"
#include "PaperSprite.h"
#include "PaperSpriteAtlas.h"

using namespace VaultAssetCheckToolReport;

namespace
{
	FString ObjPath(const FAssetData& AssetData)
	{
#if ENGINE_MAJOR_VERSION >= 5
		return AssetData.GetObjectPathString();
#else
		return AssetData.ObjectPath.ToString();
#endif
	}

	void AddClass(FARFilter& Filter, UClass* Class)
	{
#if ENGINE_MAJOR_VERSION >= 5
		Filter.ClassPaths.Add(Class->GetClassPathName());
#else
		Filter.ClassNames.Add(Class->GetFName());
#endif
	}

	void SortByPath(TArray<FAssetData>& Assets)
	{
		Assets.Sort([](const FAssetData& A, const FAssetData& B)
		{
			return ObjPath(A).Compare(ObjPath(B), ESearchCase::IgnoreCase) < 0;
		});
	}

	bool IsInGameContent(const FAssetData& AssetData)
	{
		return AssetData.PackageName.ToString().StartsWith(TEXT("/Game"));
	}
}

/**
 * Texture 리포트 태스크(멀티 테이블): Sprite → SpriteAtlas → Texture 순으로 per-asset 처리.
 * 스프라이트 패스에서 아틀라스별 스프라이트 수를 한 번에 집계해, 기존의 O(아틀라스×스프라이트) 재로드를 제거했다.
 */
class FTextureReportTask : public IVaultReportTask
{
public:
	virtual FText GetLabel() const override
	{
		return NSLOCTEXT("VaultAssetCheckTool", "TextureTaskLabel", "Texture");
	}

	virtual void Prepare(const FString& InOutputDir) override
	{
		OutputDir = InOutputDir;
		SpritesArray.Reset();
		AtlasesArray.Reset();
		TexturesArray.Reset();
		AtlasSpriteCounts.Reset();

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		FARFilter SpriteFilter;
		AddClass(SpriteFilter, UPaperSprite::StaticClass());
		SpriteFilter.PackagePaths.Add(FName("/Game"));
		SpriteFilter.bRecursivePaths = true;
		AssetRegistry.GetAssets(SpriteFilter, Sprites);

		FARFilter AtlasFilter;
		AddClass(AtlasFilter, UPaperSpriteAtlas::StaticClass());
		AtlasFilter.PackagePaths.Add(FName("/Game"));
		AtlasFilter.bRecursivePaths = true;
		AssetRegistry.GetAssets(AtlasFilter, Atlases);

		FARFilter TextureFilter;
		AddClass(TextureFilter, UTexture::StaticClass());
		TextureFilter.bRecursiveClasses = true;
		TextureFilter.PackagePaths.Add(FName("/Game"));
		TextureFilter.bRecursivePaths = true;
		AssetRegistry.GetAssets(TextureFilter, Textures);

		// 출력 순서 결정화
		SortByPath(Sprites);
		SortByPath(Atlases);
		SortByPath(Textures);
	}

	virtual int32 NumSteps() const override
	{
		return Sprites.Num() + Atlases.Num() + Textures.Num();
	}

	virtual void ProcessStep(int32 Index) override
	{
		// [0, S) 스프라이트 → [S, S+A) 아틀라스 → [S+A, S+A+T) 텍스처
		if (Index < Sprites.Num())
		{
			ProcessSprite(Sprites[Index]);
		}
		else if (Index < Sprites.Num() + Atlases.Num())
		{
			ProcessAtlas(Atlases[Index - Sprites.Num()]);
		}
		else
		{
			ProcessTexture(Textures[Index - Sprites.Num() - Atlases.Num()]);
		}
	}

	virtual FString GetStepLabel(int32 Index) const override
	{
		if (Index < Sprites.Num())
		{
			return ObjPath(Sprites[Index]);
		}
		if (Index < Sprites.Num() + Atlases.Num())
		{
			return ObjPath(Atlases[Index - Sprites.Num()]);
		}
		const int32 TexIdx = Index - Sprites.Num() - Atlases.Num();
		return Textures.IsValidIndex(TexIdx) ? ObjPath(Textures[TexIdx]) : FString();
	}

	virtual bool Finalize() override
	{
		bool bTextureSaved = SaveTable(TEXT("Texture List"), FString::Printf(TEXT("Total Texture :%d"), TexturesArray.Num()), TexturesArray, TEXT("Texture"));
		bool bSpriteSaved = SaveTable(TEXT("Sprite List"), FString::Printf(TEXT("Total Sprite :%d"), SpritesArray.Num()), SpritesArray, TEXT("Sprite"));
		bool bAtlasSaved = SaveTable(TEXT("SpriteAtlas List"), FString::Printf(TEXT("Total Sprite Atlas: %d"), AtlasesArray.Num()), AtlasesArray, TEXT("SpriteAtlas"));

		const bool bSaved = bTextureSaved && bSpriteSaved && bAtlasSaved;
		if (bSaved)
		{
			RunJsonToXlsxExe(OutputDir);
		}
		return bSaved;
	}

private:
	bool SaveTable(const FString& Title, const FString& Summary, const TArray<TSharedPtr<FJsonValue>>& Rows, const FString& Sheet)
	{
		TArray<TSharedPtr<FJsonValue>> Notices = MakeNoticeList();
		TSharedPtr<FJsonObject> RootObject = BuildSingleTableReport(Title, Title, Summary, Rows, &Notices);
		return SaveReportJson(OutputDir, TEXT("ResourceInfo"), Sheet, RootObject);
	}

	void ProcessSprite(const FAssetData& AssetData)
	{
		if (!IsInGameContent(AssetData))
		{
			return;
		}
		UPaperSprite* Sprite = Cast<UPaperSprite>(AssetData.GetAsset());
		if (!Sprite)
		{
			return;
		}

		TSharedPtr<FJsonObject> SpriteObject = MakeShareable(new FJsonObject);
		SpriteObject->SetStringField(TEXT("assetPath"), ObjPath(AssetData));
		SpriteObject->SetStringField(TEXT("assetName"), AssetData.AssetName.ToString());

		if (UTexture* SourceTexture = Sprite->GetSourceTexture())
		{
			SpriteObject->SetStringField(TEXT("sourceTexture"), SourceTexture->GetPathName());
		}
		else
		{
			SpriteObject->SetStringField(TEXT("sourceTexture"), TEXT("None"));
		}

		SpriteObject->SetNumberField(TEXT("sourceDimension_width"), Sprite->GetSourceSize().X);
		SpriteObject->SetNumberField(TEXT("sourceDimension_height"), Sprite->GetSourceSize().Y);
		SpriteObject->SetNumberField(TEXT("pixelsPerUnit"), Sprite->GetPixelsPerUnrealUnit());
		SpriteObject->SetStringField(TEXT("collisionDomain"), UEnum::GetValueAsString(Sprite->GetSpriteCollisionDomain()));
		if (Sprite->GetAtlasGroup() != nullptr)
		{
			const FString AtlasGUID = Sprite->GetAtlasGroup()->AtlasGUID.ToString();
			SpriteObject->SetStringField(TEXT("atlasGUID"), AtlasGUID);
			SpriteObject->SetBoolField(TEXT("isInAtlas"), true);
			// 아틀라스별 스프라이트 수 집계(아틀라스 패스에서 재로드 없이 사용)
			AtlasSpriteCounts.FindOrAdd(AtlasGUID)++;
		}
		else
		{
			SpriteObject->SetStringField(TEXT("atlasGUID"), TEXT(""));
			SpriteObject->SetBoolField(TEXT("isInAtlas"), false);
		}

		SpritesArray.Add(MakeShareable(new FJsonValueObject(SpriteObject)));
	}

	void ProcessAtlas(const FAssetData& AssetData)
	{
		if (!IsInGameContent(AssetData))
		{
			return;
		}
		UPaperSpriteAtlas* Atlas = Cast<UPaperSpriteAtlas>(AssetData.GetAsset());
		if (!Atlas)
		{
			return;
		}

		TSharedPtr<FJsonObject> AtlasObject = MakeShareable(new FJsonObject);
		AtlasObject->SetStringField(TEXT("assetPath"), ObjPath(AssetData));
		AtlasObject->SetStringField(TEXT("assetName"), AssetData.AssetName.ToString());

		const FString AtlasGUID = Atlas->AtlasGUID.ToString();
		AtlasObject->SetStringField(TEXT("atlasGUID"), AtlasGUID);
		AtlasObject->SetNumberField(TEXT("generatedTextureCount"), Atlas->GeneratedTextures.Num());
		AtlasObject->SetNumberField(TEXT("maxWidth"), Atlas->MaxWidth);
		AtlasObject->SetNumberField(TEXT("maxHeight"), Atlas->MaxHeight);
		AtlasObject->SetNumberField(TEXT("padding"), Atlas->Padding);
		AtlasObject->SetNumberField(TEXT("packedSpriteCount"), AtlasSpriteCounts.FindRef(AtlasGUID));
		AtlasesArray.Add(MakeShareable(new FJsonValueObject(AtlasObject)));
	}

	void ProcessTexture(const FAssetData& AssetData)
	{
		if (!IsInGameContent(AssetData))
		{
			return;
		}
		UTexture* Texture = Cast<UTexture>(AssetData.GetAsset());
		if (!Texture)
		{
			return;
		}

		TSharedPtr<FJsonObject> TextureObject = MakeShareable(new FJsonObject);

		const FString TexturePath = Texture->GetPathName();
		const FString TextureName = Texture->GetName();
		const FString TextureType = Texture->GetClass()->GetName();

		int32 SourceWidth = 0;
		int32 SourceHeight = 0;
		int32 ImportedWidth = 0;
		int32 ImportedHeight = 0;
		int32 MaxTextureSize = 0;
		FString Format = TEXT("Unknown");
		FString Compression = TEXT("Unknown");
		FString CompressionQuality = UEnum::GetValueAsString(Texture->CompressionQuality);
		float SizeKB = 0.0f;
		bool bSRGB = false;

		float DownscaleDefault = Texture->Downscale.Default;
#if ENGINE_MAJOR_VERSION >= 5
		float DownscaleMobile = Texture->Downscale.GetValueForPlatform("Mobile");
		float DownscaleDesktop = Texture->Downscale.GetValueForPlatform("Desktop");
		float DownscaleAndroid = Texture->Downscale.GetValueForPlatform("Android");
		float DownscaleIOS = Texture->Downscale.GetValueForPlatform("IOS");
		float DownscaleWindows = Texture->Downscale.GetValueForPlatform("Windows");
#else
		// UE 4.27: the per-platform accessor is GetValueForPlatformIdentifiers(FName).
		float DownscaleMobile = Texture->Downscale.GetValueForPlatformIdentifiers(FName(TEXT("Mobile")));
		float DownscaleDesktop = Texture->Downscale.GetValueForPlatformIdentifiers(FName(TEXT("Desktop")));
		float DownscaleAndroid = Texture->Downscale.GetValueForPlatformIdentifiers(FName(TEXT("Android")));
		float DownscaleIOS = Texture->Downscale.GetValueForPlatformIdentifiers(FName(TEXT("IOS")));
		float DownscaleWindows = Texture->Downscale.GetValueForPlatformIdentifiers(FName(TEXT("Windows")));
#endif
		FString DownscaleOptions = UEnum::GetValueAsString(Texture->DownscaleOptions);

		if (UTexture2D* Texture2D = Cast<UTexture2D>(Texture))
		{
#if WITH_EDITOR
			// 비동기 DDC 빌드가 끝나기 전에는 PlatformData의 SizeX/Y·Mips가 0이라 sizeKB(및 importedWidth/Height)가 0으로 나온다. 읽기 전에 빌드를 강제 완료시킨다(UE4.27/UE5 공통, WITH_EDITOR 전용).
			Texture2D->FinishCachePlatformData();
#endif

			SourceWidth = Texture2D->Source.GetSizeX();
			SourceHeight = Texture2D->Source.GetSizeY();
			ImportedWidth = Texture2D->GetSizeX();
			ImportedHeight = Texture2D->GetSizeY();
			MaxTextureSize = Texture2D->MaxTextureSize;

#if ENGINE_MAJOR_VERSION >= 5
			FTexturePlatformData* TexPlatformData = Texture2D->GetPlatformData();
#else
			FTexturePlatformData* TexPlatformData = Texture2D->PlatformData;
#endif
			// 포인터뿐 아니라 실제 빌드 완료 여부(Mips/SizeX/SizeY)도 가드 — 미완료면 CalcTextureMemorySizeEnum이 0을 반환한다.
			if (TexPlatformData && TexPlatformData->Mips.Num() > 0 && TexPlatformData->SizeX > 0 && TexPlatformData->SizeY > 0)
			{
				Format = UEnum::GetValueAsString(TexPlatformData->PixelFormat);

				// sizeKB: 밉체인 전체의 텍스처 메모리 크기를 엔진 계산 함수로 구한다.
				// 과거: Mips[0].BulkData.GetBulkDataSize()(UE4) / GetPayloadSize(0)(UE5)는
				//  (1) 0번(top) mip만 측정해 밉체인 전체가 미반영되고,
				//  (2) BulkData는 스트리밍으로 mip이 언로드되면 0을 반환(적재 상태 의존)해
				//  큰 텍스처가 0KB로 과소집계되는 버그가 있었다.
				// CalcTextureMemorySizeEnum은 포맷+밉 해상도로 계산하므로 스트리밍 상태와 무관하며 UE4/UE5 공통.
				const int64 TextureSize = Texture2D->CalcTextureMemorySizeEnum(TMC_AllMips);
				SizeKB = TextureSize / 1024.0;
			}

			Compression = UEnum::GetValueAsString(Texture2D->CompressionSettings);
			bSRGB = Texture2D->SRGB;
		}
		else if (UTextureCube* TextureCube = Cast<UTextureCube>(Texture))
		{
			SourceWidth = TextureCube->Source.GetSizeX();
			SourceHeight = TextureCube->Source.GetSizeY();
			ImportedWidth = TextureCube->GetSizeX();
			ImportedHeight = TextureCube->GetSizeY();
			MaxTextureSize = TextureCube->MaxTextureSize;
			Format = TEXT("Cube");
			Compression = UEnum::GetValueAsString(TextureCube->CompressionSettings);
			bSRGB = TextureCube->SRGB;
		}
		else if (UTextureRenderTarget2D* RenderTarget = Cast<UTextureRenderTarget2D>(Texture))
		{
			SourceWidth = RenderTarget->SizeX;
			SourceHeight = RenderTarget->SizeY;
			ImportedWidth = RenderTarget->SizeX;
			ImportedHeight = RenderTarget->SizeY;
			Format = UEnum::GetValueAsString(RenderTarget->RenderTargetFormat);
			Compression = TEXT("None (RenderTarget)");
		}
		else
		{
			ImportedWidth = Texture->GetSurfaceWidth();
			ImportedHeight = Texture->GetSurfaceHeight();
			SourceWidth = ImportedWidth;
			SourceHeight = ImportedHeight;
		}

		TextureObject->SetNumberField(TEXT("downscale_default"), DownscaleDefault);
		TextureObject->SetNumberField(TEXT("downscale_mobile"), DownscaleMobile);
		TextureObject->SetNumberField(TEXT("downscale_desktop"), DownscaleDesktop);
		TextureObject->SetNumberField(TEXT("downscale_android"), DownscaleAndroid);
		TextureObject->SetNumberField(TEXT("downscale_ios"), DownscaleIOS);
		TextureObject->SetNumberField(TEXT("downscale_windows"), DownscaleWindows);
		TextureObject->SetStringField(TEXT("downscaleOptions"), DownscaleOptions);

		TextureObject->SetStringField(TEXT("assetPath"), TexturePath);
		TextureObject->SetStringField(TEXT("assetName"), TextureName);
		TextureObject->SetStringField(TEXT("type"), TextureType);
		TextureObject->SetNumberField(TEXT("sourceWidth"), SourceWidth);
		TextureObject->SetNumberField(TEXT("sourceHeight"), SourceHeight);
		TextureObject->SetNumberField(TEXT("importedWidth"), ImportedWidth);
		TextureObject->SetNumberField(TEXT("importedHeight"), ImportedHeight);
		TextureObject->SetNumberField(TEXT("maxTextureSize"), MaxTextureSize);
		TextureObject->SetStringField(TEXT("format"), Format);
		TextureObject->SetNumberField(TEXT("sizeKB"), SizeKB);
		TextureObject->SetStringField(TEXT("compression"), Compression);
		TextureObject->SetStringField(TEXT("compressionQuality"), CompressionQuality);
		TextureObject->SetBoolField(TEXT("sRGB"), bSRGB);

		TexturesArray.Add(MakeShareable(new FJsonValueObject(TextureObject)));
	}

	FString OutputDir;
	TArray<FAssetData> Sprites;
	TArray<FAssetData> Atlases;
	TArray<FAssetData> Textures;
	TArray<TSharedPtr<FJsonValue>> SpritesArray;
	TArray<TSharedPtr<FJsonValue>> AtlasesArray;
	TArray<TSharedPtr<FJsonValue>> TexturesArray;
	TMap<FString, int32> AtlasSpriteCounts;
};

TSharedPtr<IVaultReportTask> VaultAssetCheckToolReport::MakeTextureTask()
{
	return MakeShared<FTextureReportTask>();
}

bool FVaultAssetCheckToolModule::ExportTextureReport(FString& OutputPath)
{
	return VaultAssetCheckToolReport::RunTaskSynchronously(VaultAssetCheckToolReport::MakeTextureTask(), OutputPath);
}
