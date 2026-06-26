// Copyright Epic Games, Inc. All Rights Reserved.

#include "VaultAssetCheckTool.h"
#include "Editor/VaultAssetCheckToolReportFactory.h"

// Cascade(UParticleSystem) 파티클 분석 리포트는 UE4.X 주력 시스템이다.
// Niagara가 UE5 전용인 것과 대칭으로, 본 리포트는 UE4.X에서만 본문을 컴파일한다.
// (UE5에서는 UParticleSystem이 deprecated이고 Niagara 리포트로 대체되므로 스텁으로 둔다.)
#if ENGINE_MAJOR_VERSION < 5

#include "VaultAssetCheckToolReportUtils.h"
#include "Editor/VaultAssetCheckToolSingleTableTask.h"

#include "Materials/MaterialInterface.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/Spawn/ParticleModuleSpawn.h"
#include "Particles/Lifetime/ParticleModuleLifetime.h"
#include "Distributions/DistributionFloat.h"
#include "Particles/TypeData/ParticleModuleTypeDataGpu.h"
#include "Particles/TypeData/ParticleModuleTypeDataMesh.h"
#include "Particles/TypeData/ParticleModuleTypeDataBeam2.h"
#include "Particles/TypeData/ParticleModuleTypeDataRibbon.h"
#include "Particles/TypeData/ParticleModuleTypeDataAnimTrail.h"
#include "Particles/Light/ParticleModuleLight.h"

using namespace VaultAssetCheckToolReport;

namespace
{
	// TypeDataModule 클래스로부터 이미터 렌더 타입 문자열을 결정한다.
	// TypeData가 없으면 기본 CPU 스프라이트 이미터.
	FString ResolveEmitterType(const UParticleLODLevel* LOD)
	{
		if (!LOD || !LOD->TypeDataModule)
		{
			return TEXT("Sprite");
		}
		const UObject* TD = LOD->TypeDataModule;
		if (TD->IsA(UParticleModuleTypeDataGpu::StaticClass()))       return TEXT("GPUSprite");
		if (TD->IsA(UParticleModuleTypeDataMesh::StaticClass()))      return TEXT("Mesh");
		if (TD->IsA(UParticleModuleTypeDataBeam2::StaticClass()))     return TEXT("Beam");
		if (TD->IsA(UParticleModuleTypeDataRibbon::StaticClass()))    return TEXT("Ribbon");
		if (TD->IsA(UParticleModuleTypeDataAnimTrail::StaticClass())) return TEXT("AnimTrail");
		return LOD->TypeDataModule->GetClass()->GetName();
	}
}

/**
 * Cascade 리포트 태스크: UParticleSystem 1개당 1행(이미터 상세는 중첩 배열).
 * per-asset 스텝이므로 러너가 청크 처리 + 메모리 가드 + 즉시 정지를 적용할 수 있다.
 */
class FCascadeReportTask : public FSingleTableReportTask
{
public:
	FCascadeReportTask()
		: FSingleTableReportTask(
			NSLOCTEXT("VaultAssetCheckTool", "CascadeTaskLabel", "Cascade"),
			TEXT("ResourceInfo"), TEXT("Cascade"),
			TEXT("Cascade Particle Analysis"), TEXT("Cascade List"), TEXT("Total Systems"))
	{}

protected:
	virtual void GatherAssets(IAssetRegistry& AssetRegistry, TArray<FAssetData>& OutAssets) override
	{
		FARFilter Filter;
		AddClassToFilter(Filter, UParticleSystem::StaticClass());
		Filter.PackagePaths.Add(FName("/Game"));
		Filter.bRecursivePaths = true;
		AssetRegistry.GetAssets(Filter, OutAssets);
	}

	virtual void AddReportNotices(TArray<TSharedPtr<FJsonValue>>& Notices) const override
	{
		// Niagara 리포트와 동일한 항목 의미를 Cascade 용어로 정리
		AddNotice(Notices, TEXT("Sim Target"),   TEXT("CPU/GPU 시뮬레이션 위치. GPU(TypeDataGpu)는 모바일/저사양 기기에서 호환성·비용 확인 필요"));
		AddNotice(Notices, TEXT("Est Max Particles"), TEXT("SpawnRate*MaxLifetime + Burst 기반 이론적 최대 파티클 추정치. SpawnPerUnit 모듈은 미반영(과소추정 가능). GPU 이미터는 하드웨어 캡으로 클램프됨. 실측 우선순위 트리거 + 메모리 지표"));
		AddNotice(Notices, TEXT("Is Unlit"),     TEXT("셰이딩 모델 Unlit 여부. false면 파티클 머티리얼에 라이팅 연산이 걸려 비용 상승 의심(오버드로우와 독립된 라이팅 축)"));
		AddNotice(Notices, TEXT("Uses Lights"),  TEXT("Light 모듈 사용 여부. 활성화 시 동적 라이트 비용이 급격히 상승함"));
		AddNotice(Notices, TEXT("Warmup Time"),  TEXT("0보다 클 경우 시스템 활성화 시 초기 연산 부하(프레임 히치) 유발 가능"));
		AddNotice(Notices, TEXT("LOD Count"),    TEXT("LOD 단계 수. 거리별 비용 절감을 위해 2단계 이상 권장"));
	}

	virtual TSharedPtr<FJsonObject> ExtractAsset(const FAssetData& AssetData) override
	{
		UParticleSystem* System = Cast<UParticleSystem>(AssetData.GetAsset());
		if (!System)
		{
			return nullptr;
		}

		TSharedPtr<FJsonObject> SystemObject = MakeShareable(new FJsonObject);
		SystemObject->SetStringField(TEXT("assetPath"), GetObjectPathString(AssetData));
		SystemObject->SetStringField(TEXT("assetName"), AssetData.AssetName.ToString());

		// 시스템 공통 설정
		SystemObject->SetNumberField(TEXT("emitterCount"), System->Emitters.Num());
		SystemObject->SetNumberField(TEXT("warmupTime"), System->WarmupTime);
		SystemObject->SetNumberField(TEXT("updateTime_FPS"), System->UpdateTime_FPS);

		// LOD 단계 수: Cascade는 모든 이미터가 동일 LOD 수를 공유하므로 첫 유효 이미터 기준
		int32 SystemLodCount = 0;

		bool bHasGPUEmitter = false;
		bool bHasCPUEmitter = false;
		TArray<TSharedPtr<FJsonValue>> EmitterDetailsArray;

		// 개별 이미터 상세 분석
		for (UParticleEmitter* Emitter : System->Emitters)
		{
			if (!Emitter) continue; // Emitters 배열에는 null 엔트리가 존재할 수 있음

			// 최고 품질(LOD0) 기준으로 분석한다.
			UParticleLODLevel* LOD0 = Emitter->GetLODLevel(0);

			if (SystemLodCount == 0)
			{
				SystemLodCount = Emitter->LODLevels.Num();
			}

			TSharedPtr<FJsonObject> EmitterObj = MakeShareable(new FJsonObject);
			EmitterObj->SetStringField(TEXT("name"), Emitter->EmitterName.ToString());

			// 1. 시뮬레이션 타겟 (CPU/GPU) — TypeDataGpu 모듈 유무로 판정
			const bool bIsGPU = LOD0 && LOD0->TypeDataModule && LOD0->TypeDataModule->IsA(UParticleModuleTypeDataGpu::StaticClass());
			EmitterObj->SetStringField(TEXT("simTarget"), bIsGPU ? TEXT("GPU") : TEXT("CPU"));
			if (bIsGPU) bHasGPUEmitter = true; else bHasCPUEmitter = true;

			// 2. 렌더 타입 (Sprite/GPUSprite/Mesh/Beam/Ribbon/AnimTrail)
			EmitterObj->SetStringField(TEXT("type"), ResolveEmitterType(LOD0));

			// 3. 이론적 최대 파티클 추정치 (estMaxParticles)
			//    = ceil(MaxSpawnRate * MaxLifetime) + BurstTotal
			//    PeakActiveParticles는 시뮬레이션 런타임 캐시라 에디터 로드 상태에선 항상 0이므로 사용 불가.
			//    GetMaximumSpawnRate()/GetMaxLifetime()은 MinimalAPI 클래스의 비-ENGINE_API 가상함수라
			//    플러그인 모듈에서 링크 불가(상단 CalculateMaxActiveParticleCount 선례와 동일).
			//    대신 분포 멤버(Rate/RateScale/Lifetime)를 ENGINE_API 노출된 FRawDistributionFloat::GetOutRange로 직접 평가한다.
			//    한계: UParticleModuleSpawnPerUnit(이동거리 기반 스폰)은 본 산식에 미반영되어 과소추정(0) 가능.
			float EstMaxParticles = 0.0f;
			if (LOD0)
			{
				float MaxSpawnRate = 0.0f;
				float MaxLifetime = 0.0f;
				int32 BurstTotal = 0;

				// SpawnModule: 모든 이미터가 가지는 SpawnRate/Burst 모듈 (LOD0->SpawnModule, 별도 전용 포인터)
				if (LOD0->SpawnModule)
				{
					// GetMaximumSpawnRate() 등가: Rate 분포의 max × RateScale 분포의 max
					float RateMin = 0.0f, RateMax = 0.0f;
					float ScaleMin = 0.0f, ScaleMax = 0.0f;
					LOD0->SpawnModule->Rate.GetOutRange(RateMin, RateMax);
					LOD0->SpawnModule->RateScale.GetOutRange(ScaleMin, ScaleMax);
					MaxSpawnRate = RateMax * ScaleMax;

					// Burst: 순간 스폰량 합산. CountLow(>=0)는 [CountLow..Count] 범위의 하한이므로 Count가 상한.
					for (const FParticleBurst& Burst : LOD0->SpawnModule->BurstList)
					{
						const int32 BurstMax = (Burst.CountLow >= 0)
							? FMath::Max(Burst.Count, Burst.CountLow)
							: Burst.Count;
						BurstTotal += BurstMax;
					}
				}

				// Lifetime 모듈 탐색 (LOD0->Modules 순회). GetMaxLifetime() 등가: Lifetime 분포의 max.
				for (UParticleModule* Module : LOD0->Modules)
				{
					if (UParticleModuleLifetime* LifeMod = Cast<UParticleModuleLifetime>(Module))
					{
						float LifeMin = 0.0f, LifeMax = 0.0f;
						LifeMod->Lifetime.GetOutRange(LifeMin, LifeMax);
						MaxLifetime = FMath::Max(MaxLifetime, LifeMax);
					}
				}

				EstMaxParticles = FMath::CeilToFloat(MaxSpawnRate * MaxLifetime) + BurstTotal;
			}
			// GPU 이미터는 하드웨어 고정 캡(FGPUSpriteEmitterInfo::MaxParticleCount)으로 클램프한다.
			// Build() 시점 산출값이며 UPROPERTY로 직렬화되어 에디터 로드 상태에서도 유효(>0)할 수 있다.
			// 0이면(미빌드 등) GpuCap>0 가드로 클램프를 건너뛴다.
			if (bIsGPU && LOD0 && LOD0->TypeDataModule)
			{
				if (UParticleModuleTypeDataGpu* GpuTD = Cast<UParticleModuleTypeDataGpu>(LOD0->TypeDataModule))
				{
					const int32 GpuCap = GpuTD->EmitterInfo.MaxParticleCount;
					if (GpuCap > 0)
					{
						EstMaxParticles = FMath::Min(EstMaxParticles, (float)GpuCap);
					}
				}
			}
			EmitterObj->SetNumberField(TEXT("estMaxParticles"), EstMaxParticles);

			// 4. LOD 단계 수 / 활성 여부
			EmitterObj->SetNumberField(TEXT("lodCount"), Emitter->LODLevels.Num());
			EmitterObj->SetBoolField(TEXT("bEnabled"), LOD0 ? (LOD0->bEnabled != 0) : false);

			// 5. RequiredModule 기반 정보 (머티리얼/로컬스페이스/모듈 수)
			FString MaterialName = TEXT("None");
			bool bUseLocalSpace = false;
			int32 ModuleCount = 0;
			if (LOD0)
			{
				ModuleCount = LOD0->Modules.Num();
				if (LOD0->RequiredModule)
				{
					bUseLocalSpace = (LOD0->RequiredModule->bUseLocalSpace != 0);
					if (LOD0->RequiredModule->Material)
					{
						MaterialName = LOD0->RequiredModule->Material->GetName();
					}
				}
			}
			EmitterObj->SetStringField(TEXT("material"), MaterialName);
			EmitterObj->SetBoolField(TEXT("bUseLocalSpace"), bUseLocalSpace);
			EmitterObj->SetNumberField(TEXT("moduleCount"), ModuleCount);

			// 5b. 머티리얼 Unlit 여부 — 라이팅 연산 비용 축(오버드로우와 독립). 3순위 보조 지표로 유지.
			//     (blendMode는 98.8%가 AlphaComposite 단일값이라 변별력 없어 제거, estDensity는 실측과 무관해 제거됨)
			bool bIsUnlit = false;
			if (LOD0 && LOD0->RequiredModule && LOD0->RequiredModule->Material)
			{
				bIsUnlit = LOD0->RequiredModule->Material->GetShadingModels().HasShadingModel(MSM_Unlit);
			}
			EmitterObj->SetBoolField(TEXT("isUnlit"), bIsUnlit);

			// 6. 라이트 사용 여부 — Light 모듈 존재 또는 EmitterRenderMode == ERM_LightsOnly
			bool bUsesLights = (Emitter->EmitterRenderMode == ERM_LightsOnly);
			if (LOD0 && !bUsesLights)
			{
				for (UParticleModule* Module : LOD0->Modules)
				{
					if (Module && Module->IsA(UParticleModuleLight::StaticClass()))
					{
						bUsesLights = true;
						break;
					}
				}
			}
			EmitterObj->SetBoolField(TEXT("bUsesLights"), bUsesLights);

			EmitterDetailsArray.Add(MakeShareable(new FJsonValueObject(EmitterObj)));
		}

		// 시스템 전체 시뮬레이션 요약
		FString FinalSimTarget = TEXT("Unknown");
		if (bHasGPUEmitter && bHasCPUEmitter) FinalSimTarget = TEXT("Mixed");
		else if (bHasGPUEmitter) FinalSimTarget = TEXT("GPU");
		else if (bHasCPUEmitter) FinalSimTarget = TEXT("CPU");

		SystemObject->SetStringField(TEXT("simTarget"), FinalSimTarget);
		SystemObject->SetNumberField(TEXT("lodCount"), SystemLodCount);
		SystemObject->SetArrayField(TEXT("emitters"), EmitterDetailsArray);

		return SystemObject;
	}
};

TSharedPtr<IVaultReportTask> VaultAssetCheckToolReport::MakeCascadeTask()
{
	return MakeShared<FCascadeReportTask>();
}

// 동기 경로(블루프린트 라이브러리/EUW 호환): 동일 태스크를 그 자리에서 끝까지 실행한다.
bool FVaultAssetCheckToolModule::ExportCascadeReport(FString& OutputPath)
{
	return VaultAssetCheckToolReport::RunTaskSynchronously(VaultAssetCheckToolReport::MakeCascadeTask(), OutputPath);
}

#else // ENGINE_MAJOR_VERSION < 5

// UE5 스텁: Cascade 미사용(Niagara 리포트로 대체). API/심볼 유지를 위해 nullptr 태스크 + 경고 로그만 제공한다.
TSharedPtr<IVaultReportTask> VaultAssetCheckToolReport::MakeCascadeTask()
{
	return nullptr;
}

bool FVaultAssetCheckToolModule::ExportCascadeReport(FString& OutputPath)
{
	UE_LOG(LogTemp, Warning,
		TEXT("[VaultAssetCheckTool] Cascade 리포트는 UE4.X에서만 지원됩니다. 현재 엔진(UE%d)에서는 Niagara 리포트를 사용하세요."),
		ENGINE_MAJOR_VERSION);
	return false;
}

#endif // ENGINE_MAJOR_VERSION < 5
