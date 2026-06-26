// Copyright Epic Games, Inc. All Rights Reserved.

#include "VaultAssetCheckTool.h"
#include "Editor/VaultAssetCheckToolSingleTableTask.h"
#include "Editor/VaultAssetCheckToolReportFactory.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "PhysicsEngine/BodySetup.h" // UBodySetup, FKAggregateGeom, ECollisionTraceFlag

// ===================== Skeletal Mesh =====================
class FSkeletalMeshReportTask : public FSingleTableReportTask
{
public:
	FSkeletalMeshReportTask()
		: FSingleTableReportTask(
			NSLOCTEXT("VaultAssetCheckTool", "SkeletalMeshTaskLabel", "Skeletal Mesh"),
			TEXT("ResourceInfo"), TEXT("SkeletalMesh"),
			TEXT("SkeletalMesh List"), TEXT("SkeletalMesh List"), TEXT("Total Skeletal Mesh"))
	{}

protected:
	virtual void GatherAssets(IAssetRegistry& AssetRegistry, TArray<FAssetData>& OutAssets) override
	{
		FARFilter Filter;
		AddClassToFilter(Filter, USkeletalMesh::StaticClass());
		Filter.PackagePaths.Add(FName("/Game"));
		Filter.bRecursivePaths = true;
		AssetRegistry.GetAssets(Filter, OutAssets);
	}

	virtual TSharedPtr<FJsonObject> ExtractAsset(const FAssetData& AssetData) override
	{
		USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(AssetData.GetAsset());
		if (!SkeletalMesh)
		{
			return nullptr;
		}

		FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering();
		if (!RenderData)
		{
			return nullptr;
		}

		TSharedPtr<FJsonObject> MeshObject = MakeShareable(new FJsonObject);
		MeshObject->SetStringField(TEXT("assetPath"), GetObjectPathString(AssetData));
		MeshObject->SetStringField(TEXT("assetName"), AssetData.AssetName.ToString());
		MeshObject->SetNumberField(TEXT("boneCount"), SkeletalMesh->GetRefSkeleton().GetNum());
		MeshObject->SetNumberField(TEXT("lodCount"), RenderData->LODRenderData.Num());

		TArray<TSharedPtr<FJsonValue>> LODsArray;
		for (int32 LODIndex = 0; LODIndex < RenderData->LODRenderData.Num(); ++LODIndex)
		{
			const FSkeletalMeshLODRenderData& LODData = RenderData->LODRenderData[LODIndex];
			TSharedPtr<FJsonObject> LODObject = MakeShareable(new FJsonObject);

			int32 TotalTriangleCount = 0;
			TArray<TSharedPtr<FJsonValue>> SectionsArray;

			for (int32 SectionIndex = 0; SectionIndex < LODData.RenderSections.Num(); ++SectionIndex)
			{
				const FSkelMeshRenderSection& Section = LODData.RenderSections[SectionIndex];
				TSharedPtr<FJsonObject> SectionObject = MakeShareable(new FJsonObject);

				const int32 SecVerts = Section.NumVertices;
				const int32 SecTris = Section.NumTriangles;
				const FString MaterialName
					= SkeletalMesh->GetMaterials().IsValidIndex(Section.MaterialIndex)
					? SkeletalMesh->GetMaterials()[Section.MaterialIndex].MaterialSlotName.ToString()
					: TEXT("N/A");

				TotalTriangleCount += SecTris;
				SectionObject->SetNumberField(TEXT("sectionIndex"), SectionIndex);
				SectionObject->SetNumberField(TEXT("vertices"), SecVerts);
				SectionObject->SetNumberField(TEXT("triangles"), SecTris);
				SectionObject->SetStringField(TEXT("materialName"), MaterialName);

				SectionsArray.Add(MakeShareable(new FJsonValueObject(SectionObject)));
			}

			const int32 TotalVertexCount = LODData.GetNumVertices();
			const float TotalVPT = TotalTriangleCount > 0 ? (float)TotalVertexCount / (float)TotalTriangleCount * 100.0f : 0.0f;

			LODObject->SetNumberField(TEXT("lodLevel"), LODIndex);
			LODObject->SetNumberField(TEXT("vertices"), TotalVertexCount);
			LODObject->SetNumberField(TEXT("triangles"), TotalTriangleCount);
			LODObject->SetNumberField(TEXT("vpt"), TotalVPT);
			LODObject->SetArrayField(TEXT("sectionsInfo"), SectionsArray);

			const FSkeletalMeshLODInfo* LODInfo = SkeletalMesh->GetLODInfo(LODIndex);
			if (LODInfo)
			{
				LODObject->SetNumberField(TEXT("isRecomputeNormals"), LODInfo->BuildSettings.bRecomputeNormals);
				LODObject->SetNumberField(TEXT("isRecomputeTangents"), LODInfo->BuildSettings.bRecomputeTangents);
				LODObject->SetNumberField(TEXT("useMikkTSpace"), LODInfo->BuildSettings.bUseMikkTSpace);
				LODObject->SetNumberField(TEXT("reductionPercentTriangles"), LODInfo->ReductionSettings.NumOfTrianglesPercentage);
				LODObject->SetNumberField(TEXT("reductionPercentVertices"), LODInfo->ReductionSettings.NumOfVertPercentage);
			}

			LODsArray.Add(MakeShareable(new FJsonValueObject(LODObject)));
		}
		MeshObject->SetArrayField(TEXT("lods"), LODsArray);
		return MeshObject;
	}
};

// ===================== Static Mesh =====================
class FStaticMeshReportTask : public FSingleTableReportTask
{
public:
	FStaticMeshReportTask()
		: FSingleTableReportTask(
			NSLOCTEXT("VaultAssetCheckTool", "StaticMeshTaskLabel", "Static Mesh"),
			TEXT("ResourceInfo"), TEXT("StaticMesh"),
			TEXT("[Model List]"), TEXT("[Model List]"), TEXT("Total Mesh"))
	{}

protected:
	virtual void GatherAssets(IAssetRegistry& AssetRegistry, TArray<FAssetData>& OutAssets) override
	{
		FARFilter Filter;
		AddClassToFilter(Filter, UStaticMesh::StaticClass());
		Filter.PackagePaths.Add(FName("/Game"));
		Filter.bRecursivePaths = true;
		AssetRegistry.GetAssets(Filter, OutAssets);
	}

	virtual TSharedPtr<FJsonObject> ExtractAsset(const FAssetData& AssetData) override
	{
		UStaticMesh* StaticMesh = Cast<UStaticMesh>(AssetData.GetAsset());
		if (!StaticMesh || !StaticMesh->GetRenderData())
		{
			return nullptr;
		}

		FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData();
		TSharedPtr<FJsonObject> MeshObject = MakeShareable(new FJsonObject);
		MeshObject->SetStringField(TEXT("assetPath"), GetObjectPathString(AssetData));
		MeshObject->SetStringField(TEXT("assetName"), AssetData.AssetName.ToString());
		// hasCollision: BodySetup "객체 존재" 가 아니라 실제 콜리전 형상 유무로 판정한다.
		//  - 프리미티브(Box/Sphere/Capsule/Convex 등) >= 1               → Simple 콜리전 있음
		//  - 프리미티브 0 + TraceFlag == CTF_UseComplexAsSimple          → Complex(per-poly) 콜리전 있음
		//  - 그 외                                                       → 콜리전 없음
		// (GetBodySetup()은 형상이 0개여도 거의 항상 객체를 반환해 검출력이 없다.)
		int32 CollisionPrimCount = 0;
		FString CollisionTraceFlagStr = TEXT("None");
		bool bHasCollision = false;
		if (UBodySetup* BodySetup = StaticMesh->GetBodySetup())
		{
			CollisionPrimCount = BodySetup->AggGeom.GetElementCount();

			// GetCollisionTraceFlag()는 CTF_UseDefault를 프로젝트 설정값으로 해석해 반환한다.
			const ECollisionTraceFlag TraceFlag = BodySetup->GetCollisionTraceFlag();
			switch (TraceFlag)
			{
			case CTF_UseSimpleAndComplex: CollisionTraceFlagStr = TEXT("SimpleAndComplex"); break;
			case CTF_UseSimpleAsComplex:  CollisionTraceFlagStr = TEXT("SimpleAsComplex");  break;
			case CTF_UseComplexAsSimple:  CollisionTraceFlagStr = TEXT("ComplexAsSimple");  break;
			default:                      CollisionTraceFlagStr = TEXT("Default");          break;
			}

			bHasCollision = (CollisionPrimCount > 0) || (TraceFlag == CTF_UseComplexAsSimple);
		}
		MeshObject->SetBoolField(TEXT("hasCollision"), bHasCollision);
		MeshObject->SetNumberField(TEXT("collisionPrimCount"), CollisionPrimCount);
		MeshObject->SetStringField(TEXT("collisionTraceFlag"), CollisionTraceFlagStr);
		MeshObject->SetNumberField(TEXT("lodCount"), RenderData->LODResources.Num());

		TArray<TSharedPtr<FJsonValue>> LODsArray;
		for (int32 LODIndex = 0; LODIndex < RenderData->LODResources.Num(); ++LODIndex)
		{
			const FStaticMeshLODResources& LODResource = RenderData->LODResources[LODIndex];
			TSharedPtr<FJsonObject> LODObject = MakeShareable(new FJsonObject);

			TArray<TSharedPtr<FJsonValue>> SectionsArray;
			for (int32 SectionIndex = 0; SectionIndex < LODResource.Sections.Num(); ++SectionIndex)
			{
				const FStaticMeshSection& Section = LODResource.Sections[SectionIndex];
				TSharedPtr<FJsonObject> SectionObject = MakeShareable(new FJsonObject);

				const int32 SecTris = Section.NumTriangles;
				const int32 SecVerts = (Section.MaxVertexIndex - Section.MinVertexIndex) + 1;
				const FString MaterialName
					= StaticMesh->GetStaticMaterials().IsValidIndex(Section.MaterialIndex)
					? StaticMesh->GetStaticMaterials()[Section.MaterialIndex].MaterialSlotName.ToString()
					: TEXT("N/A");

				SectionObject->SetNumberField(TEXT("sectionIndex"), SectionIndex);
				SectionObject->SetNumberField(TEXT("vertices"), SecVerts);
				SectionObject->SetNumberField(TEXT("triangles"), SecTris);
				SectionObject->SetStringField(TEXT("materialName"), MaterialName);

				SectionsArray.Add(MakeShareable(new FJsonValueObject(SectionObject)));
			}

			const int32 TotalVerts = LODResource.VertexBuffers.PositionVertexBuffer.GetNumVertices();
			const int32 TotalTris = LODResource.IndexBuffer.GetNumIndices() / 3;

			LODObject->SetNumberField(TEXT("lodLevel"), LODIndex);
			LODObject->SetNumberField(TEXT("vertices"), TotalVerts);
			LODObject->SetNumberField(TEXT("triangles"), TotalTris);
			LODObject->SetNumberField(TEXT("vpt"), TotalTris > 0 ? (float)TotalVerts / (float)TotalTris * 100.0f : 0.0f);
			LODObject->SetArrayField(TEXT("sectionsInfo"), SectionsArray);
			LODObject->SetNumberField(TEXT("uvChannels"), LODResource.VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords());

			UFbxStaticMeshImportData* ImportData = Cast<UFbxStaticMeshImportData>(StaticMesh->AssetImportData);
			if (ImportData != nullptr)
			{
				LODObject->SetNumberField(TEXT("isRecomputeNormals"), ImportData->NormalImportMethod == EFBXNormalImportMethod::FBXNIM_ComputeNormals);
				LODObject->SetNumberField(TEXT("useMikkTSpace"), ImportData->NormalGenerationMethod == EFBXNormalGenerationMethod::MikkTSpace);
			}

			FMeshReductionSettings ReductionSettings = StaticMesh->GetReductionSettings(LODIndex);
			LODObject->SetNumberField(TEXT("reductionPercentTriangles"), ReductionSettings.PercentTriangles);
			LODObject->SetNumberField(TEXT("reductionPercentVertices"), ReductionSettings.PercentVertices);

			LODsArray.Add(MakeShareable(new FJsonValueObject(LODObject)));
		}
		MeshObject->SetArrayField(TEXT("lods"), LODsArray);
		return MeshObject;
	}
};

TSharedPtr<IVaultReportTask> VaultAssetCheckToolReport::MakeSkeletalMeshTask()
{
	return MakeShared<FSkeletalMeshReportTask>();
}

TSharedPtr<IVaultReportTask> VaultAssetCheckToolReport::MakeStaticMeshTask()
{
	return MakeShared<FStaticMeshReportTask>();
}

bool FVaultAssetCheckToolModule::ExportSkeletalMeshReport(FString& OutputPath)
{
	return VaultAssetCheckToolReport::RunTaskSynchronously(VaultAssetCheckToolReport::MakeSkeletalMeshTask(), OutputPath);
}

bool FVaultAssetCheckToolModule::ExportStaticMeshReport(FString& OutputPath)
{
	return VaultAssetCheckToolReport::RunTaskSynchronously(VaultAssetCheckToolReport::MakeStaticMeshTask(), OutputPath);
}
