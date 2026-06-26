// Copyright Epic Games, Inc. All Rights Reserved.

#include "VaultAssetCheckTool.h"
#include "Editor/VaultAssetCheckToolSingleTableTask.h"
#include "Editor/VaultAssetCheckToolReportFactory.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableText.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/RichTextBlock.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/RetainerBox.h"
#include "Components/InvalidationBox.h"
#include "Editor/UMGEditor/Public/WidgetBlueprint.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInterface.h"

struct FWidgetChildInfo
{
	FString Name;
	FString Hierarchy;
	FString ClassName;
	TArray<FString> Textures;
	TArray<FString> Materials;
	TArray<FString> Fonts;
	FVector2D Size;
	FVector2D Position;
	int32 Depth;
	FString Visibility;
	float RenderOpacity;
	bool IsEnabled;
	int32 ZOrder;
	float FontSize;
	int Phase;
	int PhaseCount;
	FString RenderOnInvalidate;
	FString RenderOnPhase;
	FVector2D TextureSize;
	FVector2D SizeRatio;
	// overRate 측정 가능 여부/사유. Measured / NoCanvasSize / NotTexture / NotImage.
	FString OverRateStatus = TEXT("NotImage");
	// CanvasPanelSlot의 Size To Content(AutoSize). true면 GetSize()가 런타임 실제 크기가 아닌
	// Details 입력 고정값을 반환하므로 overRate 오탐의 원인이 된다(분석 단계에서 이 플래그로 거른다).
	bool bAutoSize;

	FWidgetChildInfo()
		: Size(FVector2D::ZeroVector)
		, Position(FVector2D::ZeroVector)
		, Depth(0)
		, RenderOpacity(1.0f)
		, ZOrder(0)
		, FontSize(0.0f)
		, TextureSize(FVector2D::ZeroVector)
		, SizeRatio(FVector2D::ZeroVector)
		, bAutoSize(false)
	{
	}
};

struct FWidgetInfo
{
	int32 TotalWidgetCount = 0;
	int32 MaxDepth = 0;
	TArray<FWidgetChildInfo> AllChildren;

	void CollectWidgetInfo(UWidget* Widget, int32 Depth, const FString& ParentHierarchy)
	{
		if (!Widget)
		{
			return;
		}

		TotalWidgetCount++;
		MaxDepth = FMath::Max(MaxDepth, Depth);

		FWidgetChildInfo ChildInfo;
		ChildInfo.Name = Widget->GetName();
		ChildInfo.ClassName = Widget->GetClass()->GetPathName();
		ChildInfo.Hierarchy = ParentHierarchy.IsEmpty() ? ChildInfo.Name : (ParentHierarchy + TEXT("/") + ChildInfo.Name);
		ChildInfo.Depth = Depth;
		ChildInfo.Visibility = UEnum::GetValueAsString(Widget->GetVisibility());
		ChildInfo.IsEnabled = Widget->GetIsEnabled();
		ChildInfo.RenderOpacity = Widget->GetRenderOpacity();
		auto* retainerBox = Cast<URetainerBox>(Widget);
		if (retainerBox != nullptr)
		{
			ChildInfo.RenderOnInvalidate = LexToString(retainerBox->RenderOnInvalidation);
			ChildInfo.RenderOnPhase = LexToString(retainerBox->RenderOnPhase);
			ChildInfo.Phase = retainerBox->Phase;
			ChildInfo.PhaseCount = retainerBox->PhaseCount;
		}
		else
		{
			ChildInfo.RenderOnInvalidate = TEXT("-");
			ChildInfo.RenderOnPhase = TEXT("-");
			ChildInfo.Phase = -1;
			ChildInfo.PhaseCount = -1;
		}

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			ChildInfo.Size = CanvasSlot->GetSize();
			ChildInfo.Position = CanvasSlot->GetPosition();
			ChildInfo.ZOrder = CanvasSlot->GetZOrder();
			ChildInfo.bAutoSize = CanvasSlot->GetAutoSize(); // UE4.26+ / UE5 공통 getter
		}

		if (UImage* Image = Cast<UImage>(Widget))
		{
			// 이미지지만 텍스처가 아직 확인되지 않은 상태(머티리얼/리소스 없음 포함)의 기본값.
			ChildInfo.OverRateStatus = TEXT("NotTexture");
#if ENGINE_MAJOR_VERSION >= 5
			const FSlateBrush& Brush = Image->GetBrush();
#else
			const FSlateBrush& Brush = Image->Brush;
#endif
			UObject* ResourceObject = Brush.GetResourceObject();
			if (ResourceObject)
			{
				if (UMaterialInterface* Material = Cast<UMaterialInterface>(ResourceObject))
				{
					ChildInfo.Materials.Add(Material->GetPathName());
					// 머티리얼은 원본 픽셀 크기 개념이 없어 overRate 측정 대상이 아님 → NotTexture 유지.
				}
				else if (UTexture* Texture = Cast<UTexture>(ResourceObject))
				{
					ChildInfo.Textures.Add(Texture->GetPathName());

					const float TexW = Texture->GetSurfaceWidth();
					const float TexH = Texture->GetSurfaceHeight();
					ChildInfo.TextureSize = FVector2D(TexW, TexH);

					if (ChildInfo.Size.X > 0.0f && ChildInfo.Size.Y > 0.0f)
					{
						ChildInfo.SizeRatio.X = TexW / ChildInfo.Size.X;
						ChildInfo.SizeRatio.Y = TexH / ChildInfo.Size.Y;
						ChildInfo.OverRateStatus = TEXT("Measured");
					}
					else
					{
						// 텍스처는 있으나 Canvas 슬롯 표시 크기를 정적으로 알 수 없음(박스/그리드 등).
						ChildInfo.OverRateStatus = TEXT("NoCanvasSize");
					}
				}
			}
		}

		if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
		{
#if ENGINE_MAJOR_VERSION >= 5
			const FSlateFontInfo& FontInfo = TextBlock->GetFont();
#else
			const FSlateFontInfo& FontInfo = TextBlock->Font;
#endif
			if (FontInfo.FontObject)
			{
				ChildInfo.Fonts.Add(FontInfo.FontObject->GetPathName());
			}
			ChildInfo.FontSize = FontInfo.Size;
		}

		if (UButton* Button = Cast<UButton>(Widget))
		{
			TArray<const FSlateBrush*> Brushes;
#if ENGINE_MAJOR_VERSION >= 5
			Button->GetStyle().GetResources(Brushes);
#else
			Button->WidgetStyle.GetResources(Brushes);
#endif
			for (const FSlateBrush* Brush : Brushes)
			{
				if (!Brush)
				{
					continue;
				}

				if (UObject* ResourceObject = Brush->GetResourceObject())
				{
					if (UMaterialInterface* Material = Cast<UMaterialInterface>(ResourceObject))
					{
						ChildInfo.Materials.AddUnique(Material->GetPathName());
					}
					else if (UTexture* Texture = Cast<UTexture>(ResourceObject))
					{
						ChildInfo.Textures.AddUnique(Texture->GetPathName());
					}
				}
			}
		}

		if (UBorder* Border = Cast<UBorder>(Widget))
		{
			if (UObject* BrushResource = Border->Background.GetResourceObject())
			{
				if (UMaterialInterface* Material = Cast<UMaterialInterface>(BrushResource))
				{
					ChildInfo.Materials.AddUnique(Material->GetPathName());
				}
				else if (UTexture* Texture = Cast<UTexture>(BrushResource))
				{
					ChildInfo.Textures.AddUnique(Texture->GetPathName());
				}
			}
		}

		if (UProgressBar* ProgressBar = Cast<UProgressBar>(Widget))
		{
#if ENGINE_MAJOR_VERSION >= 5
			const FProgressBarStyle& PBStyle = ProgressBar->GetWidgetStyle();
#else
			const FProgressBarStyle& PBStyle = ProgressBar->WidgetStyle;
#endif
			if (UObject* FillResource = PBStyle.FillImage.GetResourceObject())
			{
				if (UMaterialInterface* Material = Cast<UMaterialInterface>(FillResource))
				{
					ChildInfo.Materials.AddUnique(Material->GetPathName());
				}
				else if (UTexture* Texture = Cast<UTexture>(FillResource))
				{
					ChildInfo.Textures.AddUnique(Texture->GetPathName());
				}
			}

			if (UObject* BgResource = PBStyle.BackgroundImage.GetResourceObject())
			{
				if (UMaterialInterface* Material = Cast<UMaterialInterface>(BgResource))
				{
					ChildInfo.Materials.AddUnique(Material->GetPathName());
				}
				else if (UTexture* Texture = Cast<UTexture>(BgResource))
				{
					ChildInfo.Textures.AddUnique(Texture->GetPathName());
				}
			}
		}

		if (USlider* Slider = Cast<USlider>(Widget))
		{
#if ENGINE_MAJOR_VERSION >= 5
			const FSliderStyle& SliderStyle = Slider->GetWidgetStyle();
#else
			const FSliderStyle& SliderStyle = Slider->WidgetStyle;
#endif
			if (UObject* ThumbResource = SliderStyle.NormalThumbImage.GetResourceObject())
			{
				if (UMaterialInterface* Material = Cast<UMaterialInterface>(ThumbResource))
				{
					ChildInfo.Materials.AddUnique(Material->GetPathName());
				}
				else if (UTexture* Texture = Cast<UTexture>(ThumbResource))
				{
					ChildInfo.Textures.AddUnique(Texture->GetPathName());
				}
			}

			if (UObject* BarResource = SliderStyle.NormalBarImage.GetResourceObject())
			{
				if (UMaterialInterface* Material = Cast<UMaterialInterface>(BarResource))
				{
					ChildInfo.Materials.AddUnique(Material->GetPathName());
				}
				else if (UTexture* Texture = Cast<UTexture>(BarResource))
				{
					ChildInfo.Textures.AddUnique(Texture->GetPathName());
				}
			}
		}

		if (UEditableText* EditableText = Cast<UEditableText>(Widget))
		{
#if ENGINE_MAJOR_VERSION >= 5
			const FSlateFontInfo& ETFont = EditableText->GetFont();
#else
			const FSlateFontInfo& ETFont = EditableText->WidgetStyle.Font;
#endif
			if (ETFont.FontObject)
			{
				ChildInfo.Fonts.AddUnique(ETFont.FontObject->GetPathName());
			}
		}

		if (URichTextBlock* RichTextBlock = Cast<URichTextBlock>(Widget))
		{
#if ENGINE_MAJOR_VERSION >= 5
			if (RichTextBlock->GetTextStyleSet())
			{
				ChildInfo.Fonts.AddUnique(RichTextBlock->GetTextStyleSet()->GetPathName());
			}
#else
			// UE 4.27: URichTextBlock::TextStyleSet is protected with no public getter,
			// so it cannot be read here; skip reporting it for the engine 4 build.
			(void)RichTextBlock;
#endif
		}

		AllChildren.Add(ChildInfo);

		if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget))
		{
			for (int32 i = 0; i < PanelWidget->GetChildrenCount(); i++)
			{
				UWidget* ChildWidget = PanelWidget->GetChildAt(i);
				if (ChildWidget)
				{
					CollectWidgetInfo(ChildWidget, Depth + 1, ChildInfo.Hierarchy);
				}
			}
		}
	}
};

/** UMG 리포트 태스크: WidgetBlueprint 1개당 1행(내부 위젯 트리 포함). */
class FUMGReportTask : public FSingleTableReportTask
{
public:
	FUMGReportTask()
		: FSingleTableReportTask(
			NSLOCTEXT("VaultAssetCheckTool", "UMGTaskLabel", "UMG"),
			TEXT("ResourceInfo"), TEXT("UMG"),
			TEXT("GUIPrefab List"), TEXT("GUIPrefab List"), TEXT("Total UGUIPrefab"))
	{}

protected:
	virtual void GatherAssets(IAssetRegistry& AssetRegistry, TArray<FAssetData>& OutAssets) override
	{
		FARFilter Filter;
		AddClassToFilter(Filter, UWidgetBlueprint::StaticClass());
		Filter.PackagePaths.Add(FName("/Game"));
		Filter.bRecursivePaths = true;
		AssetRegistry.GetAssets(Filter, OutAssets);
	}

	virtual TSharedPtr<FJsonObject> ExtractAsset(const FAssetData& AssetData) override
	{
		UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(AssetData.GetAsset());
		if (!WidgetBP)
		{
			return nullptr;
		}

		FWidgetInfo WidgetInfo;
		FString RootWidgetType = TEXT("None");

		UWidget* RootWidget = WidgetBP->WidgetTree ? WidgetBP->WidgetTree->RootWidget : nullptr;
		if (RootWidget)
		{
			RootWidgetType = RootWidget->GetClass()->GetName();
			WidgetInfo.CollectWidgetInfo(RootWidget, 0, TEXT(""));
		}

		TSharedPtr<FJsonObject> WidgetObject = MakeShareable(new FJsonObject);
		WidgetObject->SetStringField(TEXT("assetPath"), GetObjectPathString(AssetData));
		WidgetObject->SetStringField(TEXT("assetName"), AssetData.AssetName.ToString());
		WidgetObject->SetNumberField(TEXT("totalWidgets"), WidgetInfo.TotalWidgetCount);
		WidgetObject->SetNumberField(TEXT("maxDepth"), WidgetInfo.MaxDepth);
		WidgetObject->SetStringField(TEXT("rootWidgetType"), RootWidgetType);

		TArray<TSharedPtr<FJsonValue>> ChildrenArray;
		for (const FWidgetChildInfo& Child : WidgetInfo.AllChildren)
		{
			TSharedPtr<FJsonObject> ChildObject = MakeShareable(new FJsonObject);
			ChildObject->SetStringField(TEXT("name"), Child.Name);
			ChildObject->SetStringField(TEXT("hierarchy"), Child.Hierarchy);
			ChildObject->SetStringField(TEXT("className"), Child.ClassName);
			ChildObject->SetNumberField(TEXT("depth"), Child.Depth);
			ChildObject->SetNumberField(TEXT("renderOpacity"), Child.RenderOpacity);
			ChildObject->SetNumberField(TEXT("zOrder"), Child.ZOrder);
			ChildObject->SetStringField(TEXT("Visibility"), Child.Visibility);
			ChildObject->SetBoolField(TEXT("IsEnabled"), Child.IsEnabled);
			ChildObject->SetStringField(TEXT("RenderOnPhase"), Child.RenderOnPhase);
			ChildObject->SetStringField(TEXT("RenderOnInvalidate"), Child.RenderOnInvalidate);
			ChildObject->SetNumberField(TEXT("Phase"), Child.Phase);
			ChildObject->SetNumberField(TEXT("PhaseCount"), Child.PhaseCount);
			ChildObject->SetNumberField(TEXT("position_x"), Child.Position.X);
			ChildObject->SetNumberField(TEXT("position_y"), Child.Position.Y);
			ChildObject->SetNumberField(TEXT("size_x"), Child.Size.X);
			ChildObject->SetNumberField(TEXT("size_y"), Child.Size.Y);
			ChildObject->SetBoolField(TEXT("autoSize"), Child.bAutoSize);
			if (Child.FontSize > 0.0f)
			{
				ChildObject->SetNumberField(TEXT("fontSize"), Child.FontSize);
			}

			ChildObject->SetNumberField(TEXT("textureSize_x"), Child.TextureSize.X);
			ChildObject->SetNumberField(TEXT("textureSize_y"), Child.TextureSize.Y);
			// overRate: 텍스처 면적 / 표시 면적 의 배율(%). 100 = 원본과 1:1, 400 = 면적 4배 과대.
			// 면적비 = (TexW*TexH)/(DispW*DispH) = SizeRatio.X * SizeRatio.Y (두 축 배율의 곱).
			// 측정 불가/대상 아님은 -1로 내보내고, 사유는 overRateStatus 컬럼으로 구분한다.
			const bool bOverRateMeasured = Child.OverRateStatus == TEXT("Measured");
			ChildObject->SetNumberField(TEXT("overRate"),
				bOverRateMeasured ? Child.SizeRatio.X * Child.SizeRatio.Y * 100.0f : -1.0);
			ChildObject->SetStringField(TEXT("overRateStatus"), Child.OverRateStatus);

			TArray<TSharedPtr<FJsonValue>> TexturesArray;
			for (const FString& Texture : Child.Textures)
			{
				TexturesArray.Add(MakeShareable(new FJsonValueString(Texture)));
			}
			ChildObject->SetArrayField(TEXT("textures"), TexturesArray);

			TArray<TSharedPtr<FJsonValue>> MaterialsArray;
			for (const FString& Material : Child.Materials)
			{
				MaterialsArray.Add(MakeShareable(new FJsonValueString(Material)));
			}
			ChildObject->SetArrayField(TEXT("materials"), MaterialsArray);

			TArray<TSharedPtr<FJsonValue>> FontsArray;
			for (const FString& Font : Child.Fonts)
			{
				FontsArray.Add(MakeShareable(new FJsonValueString(Font)));
			}
			ChildObject->SetArrayField(TEXT("fonts"), FontsArray);

			ChildrenArray.Add(MakeShareable(new FJsonValueObject(ChildObject)));
		}
		WidgetObject->SetArrayField(TEXT("widgets"), ChildrenArray);
		return WidgetObject;
	}
};

TSharedPtr<IVaultReportTask> VaultAssetCheckToolReport::MakeUMGTask()
{
	return MakeShared<FUMGReportTask>();
}

bool FVaultAssetCheckToolModule::ExportUMGAssetReport(FString& OutputPath)
{
	return VaultAssetCheckToolReport::RunTaskSynchronously(VaultAssetCheckToolReport::MakeUMGTask(), OutputPath);
}
