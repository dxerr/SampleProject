// Copyright Epic Games, Inc. All Rights Reserved.

#include "VaultAssetCheckTool.h"
#include "Editor/VaultAssetCheckToolReportFactory.h"

// Niagara 분석 리포트는 UE5 이상에서만 지원한다.
// UE4.X에서는 Niagara 모듈에 하드 의존하지 않도록(런타임 DLL 로드 실패 방지) 본문 전체를 스텁으로 컴파일한다.
// (UE4.X 주력 파티클은 Cascade이며 VaultAssetCheckTool_ReportCascade.cpp가 대응한다.)
#if ENGINE_MAJOR_VERSION >= 5

#include "VaultAssetCheckToolReportUtils.h"
#include "Editor/VaultAssetCheckToolSingleTableTask.h"

#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraRendererProperties.h" // 렌더러 분석을 위해 필요
#include "Materials/MaterialInterface.h" // GetBlendMode/GetShadingModels

using namespace VaultAssetCheckToolReport;

namespace
{
	// EBlendMode → 표시 문자열 (Cascade 리포트와 동일 매핑)
	FString BlendModeToString(EBlendMode Mode)
	{
		switch (Mode)
		{
			case BLEND_Opaque:         return TEXT("Opaque");
			case BLEND_Masked:         return TEXT("Masked");
			case BLEND_Translucent:    return TEXT("Translucent");
			case BLEND_Additive:       return TEXT("Additive");
			case BLEND_Modulate:       return TEXT("Modulate");
			case BLEND_AlphaComposite: return TEXT("AlphaComposite");
			case BLEND_AlphaHoldout:   return TEXT("AlphaHoldout");
			default:                   return TEXT("Other");
		}
	}

	// 오버드로우 비용 근사 순위(클수록 비쌈). 한 이미터에 렌더러가 여럿일 때 최댓값을 대표값으로 쓴다.
	int32 BlendModeCostRank(EBlendMode Mode)
	{
		switch (Mode)
		{
			case BLEND_Opaque:         return 0;
			case BLEND_Masked:         return 1;
			case BLEND_Modulate:       return 2;
			case BLEND_AlphaComposite:
			case BLEND_AlphaHoldout:   return 3;
			case BLEND_Translucent:    return 4;
			case BLEND_Additive:       return 5;
			default:                   return 2;
		}
	}
}

/**
 * Niagara 리포트 태스크: UNiagaraSystem 1개당 1행(이미터 상세는 중첩 배열).
 * per-asset 스텝이므로 러너가 청크 처리 + 메모리 가드 + 즉시 정지를 적용할 수 있다.
 */
class FNiagaraReportTask : public FSingleTableReportTask
{
public:
	FNiagaraReportTask()
		: FSingleTableReportTask(
			NSLOCTEXT("VaultAssetCheckTool", "NiagaraTaskLabel", "Niagara"),
			TEXT("ResourceInfo"), TEXT("Niagara"),
			TEXT("Niagara System Analysis"), TEXT("Niagara List"), TEXT("Total Systems"))
	{}

protected:
	virtual void GatherAssets(IAssetRegistry& AssetRegistry, TArray<FAssetData>& OutAssets) override
	{
		FARFilter Filter;
		AddClassToFilter(Filter, UNiagaraSystem::StaticClass());
		Filter.PackagePaths.Add(FName("/Game"));
		Filter.bRecursivePaths = true;
		AssetRegistry.GetAssets(Filter, OutAssets);
	}

	virtual void AddReportNotices(TArray<TSharedPtr<FJsonValue>>& Notices) const override
	{
		AddNotice(Notices, TEXT("Sim Target"),     TEXT("CPU/GPU 시뮬레이션 위치. GPU는 모바일/저사양 기기에서 확인 필요"));
		AddNotice(Notices, TEXT("Max Particles"),  TEXT("이미터에서 생성 가능한 최대 파티클 수 추정치(GetMaxParticleCountEstimate). 메모리 및 연산 부하 지표"));
		AddNotice(Notices, TEXT("Blend Mode"),     TEXT("렌더 머티리얼 블렌드 모드(여러 렌더러 중 가장 비싼 값). Additive/Translucent는 오버드로우 비용이 높음. 최종 판정은 실측 권장"));
		AddNotice(Notices, TEXT("Is Unlit"),       TEXT("이미터 머티리얼이 모두 Unlit인지 여부. false면 라이팅 연산이 걸려 비용 상승 의심"));
		AddNotice(Notices, TEXT("Allocation Mode"), TEXT("파티클 메모리 할당 방식. ManualEstimate/FixedCount면 PreAllocationCount가 정적 상한 지표(Niagara는 고정 GPU 캡 없음)"));
		AddNotice(Notices, TEXT("Uses Lights"),    TEXT("라이트 렌더러 사용 여부. 활성화 시 렌더링 비용이 급격히 상승함"));
		AddNotice(Notices, TEXT("Warmup Time"),    TEXT("0보다 클 경우 시스템 활성화 시 초기 연산 부하(프레임 드랍) 유발 가능"));
	}

	virtual TSharedPtr<FJsonObject> ExtractAsset(const FAssetData& AssetData) override
	{
		UNiagaraSystem* System = Cast<UNiagaraSystem>(AssetData.GetAsset());
		if (!System)
		{
			return nullptr;
		}

		TSharedPtr<FJsonObject> NiagaraObject = MakeShareable(new FJsonObject);
		NiagaraObject->SetStringField(TEXT("assetPath"), GetObjectPathString(AssetData));
		NiagaraObject->SetStringField(TEXT("assetName"), AssetData.AssetName.ToString());

		// 시스템 공통 설정
		NiagaraObject->SetNumberField(TEXT("emitterCount"), System->GetEmitterHandles().Num());
		NiagaraObject->SetNumberField(TEXT("warmupTime"), System->GetWarmupTime());

		bool bHasGPUEmitter = false;
		bool bHasCPUEmitter = false;
		TArray<TSharedPtr<FJsonValue>> EmitterDetailsArray;

		// 개별 이미터 상세 분석
		for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
		{
			FVersionedNiagaraEmitter Emitter = Handle.GetInstance();
			FVersionedNiagaraEmitterData* EmitterData = Emitter.GetEmitterData();
			if (!EmitterData)
			{
				continue;
			}

			TSharedPtr<FJsonObject> EmitterObj = MakeShareable(new FJsonObject);
			EmitterObj->SetStringField(TEXT("name"), Handle.GetName().ToString());

			// 1. 시뮬레이션 타겟 (CPU/GPU)
			const bool bIsGPU = (EmitterData->SimTarget == ENiagaraSimTarget::GPUComputeSim);
			EmitterObj->SetStringField(TEXT("simTarget"), bIsGPU ? TEXT("GPU") : TEXT("CPU"));
			if (bIsGPU) bHasGPUEmitter = true; else bHasCPUEmitter = true;

			// 2. 최대 파티클 개수
			EmitterObj->SetNumberField(TEXT("maxParticles"), EmitterData->GetMaxParticleCountEstimate());

			// 3. 렌더러 / 라이트 사용 / 머티리얼(블렌드·Unlit) 체크 — 단일 순회로 처리
			bool bUsesLights = false;
			int32 WorstBlendRank = -1;
			FString BlendModeStr = TEXT("None");
			bool bFoundMaterial = false;
			bool bAnyLit = false;
			TArray<TSharedPtr<FJsonValue>> RendererNames;
			for (UNiagaraRendererProperties* Renderer : EmitterData->GetRenderers())
			{
				if (Renderer && Renderer->GetIsEnabled())
				{
					const FString RendererClassName = Renderer->GetClass()->GetName();
					RendererNames.Add(MakeShareable(new FJsonValueString(RendererClassName)));

					if (RendererClassName.Contains(TEXT("Light")))
					{
						bUsesLights = true;
					}

					// 렌더러별 머티리얼 순회. 한 이미터에 Sprite/Mesh/Ribbon 렌더러가 복수 존재 가능.
					// InEmitter=nullptr는 Sprite/Mesh/Ribbon 구현 모두 null 가드 후 에셋 기본 머티리얼로 폴백(에디터 정적 추출 안전, 엔진 소스 확인 완료).
					TArray<UMaterialInterface*> Mats;
					Renderer->GetUsedMaterials(nullptr, Mats);
					for (UMaterialInterface* Mat : Mats)
					{
						if (!Mat) continue;
						bFoundMaterial = true;
						const EBlendMode Blend = Mat->GetBlendMode();
						const int32 Rank = BlendModeCostRank(Blend);
						if (Rank > WorstBlendRank)
						{
							WorstBlendRank = Rank;
							BlendModeStr = BlendModeToString(Blend);
						}
						if (!Mat->GetShadingModels().HasShadingModel(MSM_Unlit))
						{
							bAnyLit = true;
						}
					}
				}
			}
			EmitterObj->SetArrayField(TEXT("renderers"), RendererNames);
			EmitterObj->SetBoolField(TEXT("bUsesLights"), bUsesLights);

			// 4. 블렌드 모드 / Unlit 여부 — 오버드로우·라이팅 비용 1차 대리지표.
			//    blendMode: 여러 렌더러 머티리얼 중 가장 비싼 블렌드(Additive/Translucent 우선)를 대표값으로.
			//    isUnlit: 발견된 머티리얼이 모두 Unlit일 때만 true(하나라도 라이팅 걸리면 false → 비용 의심).
			EmitterObj->SetStringField(TEXT("blendMode"), BlendModeStr);
			EmitterObj->SetBoolField(TEXT("isUnlit"), bFoundMaterial && !bAnyLit);

			// 5. 정적 할당 추정 — Niagara는 Cascade의 고정 GPU 캡 개념이 없으므로(컴퓨트 동적)
			//    AllocationMode + PreAllocationCount가 대응 정적 지표. AutomaticEstimate면 엔진 자동(PreAllocationCount=0).
			const FString AllocModeStr =
				(EmitterData->AllocationMode == EParticleAllocationMode::AutomaticEstimate) ? TEXT("AutomaticEstimate")
				: (EmitterData->AllocationMode == EParticleAllocationMode::ManualEstimate) ? TEXT("ManualEstimate")
				: TEXT("FixedCount");
			EmitterObj->SetStringField(TEXT("allocationMode"), AllocModeStr);
			EmitterObj->SetNumberField(TEXT("preAllocationCount"), EmitterData->PreAllocationCount);

			EmitterDetailsArray.Add(MakeShareable(new FJsonValueObject(EmitterObj)));
		}

		// 시스템 전체 시뮬레이션 요약
		FString FinalSimTarget = TEXT("Unknown");
		if (bHasGPUEmitter && bHasCPUEmitter) FinalSimTarget = TEXT("Mixed");
		else if (bHasGPUEmitter) FinalSimTarget = TEXT("GPU");
		else if (bHasCPUEmitter) FinalSimTarget = TEXT("CPU");

		NiagaraObject->SetStringField(TEXT("simTarget"), FinalSimTarget);
		NiagaraObject->SetArrayField(TEXT("emitters"), EmitterDetailsArray);

		return NiagaraObject;
	}
};

TSharedPtr<IVaultReportTask> VaultAssetCheckToolReport::MakeNiagaraTask()
{
	return MakeShared<FNiagaraReportTask>();
}

// 동기 경로(블루프린트 라이브러리/EUW 호환): 동일 태스크를 그 자리에서 끝까지 실행한다.
bool FVaultAssetCheckToolModule::ExportNiagaraReport(FString& OutputPath)
{
	return VaultAssetCheckToolReport::RunTaskSynchronously(VaultAssetCheckToolReport::MakeNiagaraTask(), OutputPath);
}

#else // ENGINE_MAJOR_VERSION >= 5

// UE4.X 스텁: Niagara 미지원. API/심볼 유지를 위해 nullptr 태스크 + 경고 로그만 제공한다.
TSharedPtr<IVaultReportTask> VaultAssetCheckToolReport::MakeNiagaraTask()
{
	return nullptr;
}

bool FVaultAssetCheckToolModule::ExportNiagaraReport(FString& OutputPath)
{
	UE_LOG(LogTemp, Warning,
		TEXT("[VaultAssetCheckTool] Niagara 리포트는 UE5 이상에서만 지원됩니다. 현재 엔진(UE%d)에서는 Cascade 리포트를 사용하세요."),
		ENGINE_MAJOR_VERSION);
	return false;
}

#endif // ENGINE_MAJOR_VERSION >= 5
