// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ExMusicPhaseDataAsset.generated.h"

/**
 * FExMusicLayerConfig
 * 단일 음악 레이어의 설정 정보
 * MetaSound 파라미터와 1:1 매핑되어 볼륨을 실시간 제어
 */
USTRUCT(BlueprintType)
struct FExMusicLayerConfig
{
	GENERATED_BODY()

	/** 레이어 식별자 (예: "Base", "Melody", "Percussion") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music|Layer")
	FName LayerName;

	/** MetaSound 그래프 내 노출된 볼륨 파라미터 이름 (예: "LayerVolume_Base") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music|Layer")
	FName VolumeParameterName;

	/** 초기 볼륨 (0~1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music|Layer", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InitialVolume = 0.f;
};

/**
 * FExMusicPhasePreset
 * Phase별 레이어 볼륨 프리셋 정의
 * 각 Phase에서 어떤 레이어를 어떤 볼륨으로 재생할지 결정
 */
USTRUCT(BlueprintType)
struct FExMusicPhasePreset
{
	GENERATED_BODY()

	/** Phase 식별 태그 (예: Ex.Music.Phase.Running) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music|Phase")
	FGameplayTag PhaseTag;

	/**
	 * 레이어명 → 목표 볼륨 매핑
	 * 이 맵에 없는 레이어는 현재 볼륨을 유지합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music|Phase")
	TMap<FName, float> LayerVolumes;

	/** 전환 시간 (초): 이전 Phase에서 이 Phase로 얼마나 부드럽게 전환할지 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music|Phase", meta = (ClampMin = "0.0"))
	float TransitionDuration = 2.0f;
};

/**
 * UExMusicPhaseDataAsset
 * Phase별 레이어 볼륨 프리셋을 데이터 드리븐 방식으로 관리하는 데이터 에셋
 * 
 * 에디터에서 DA_ExMusicPhase 등의 이름으로 생성하여
 * 각 Phase(Warmup, Running, Climax, Cooldown)의 레이어 구성을 정의합니다.
 */
UCLASS(BlueprintType)
class EXCORERUNTIME_API UExMusicPhaseDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 레이어 구성 정의 (MetaSound 파라미터와 매핑) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music")
	TArray<FExMusicLayerConfig> LayerConfigs;

	/** Phase별 프리셋 목록 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music")
	TArray<FExMusicPhasePreset> PhasePresets;

	/** 초기 Phase 태그 (게임 시작 시 자동 적용) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music")
	FGameplayTag InitialPhaseTag;

	/**
	 * Phase 태그로 프리셋 검색
	 * @param PhaseTag 검색할 Phase 태그
	 * @return 해당 프리셋 포인터 (없으면 nullptr)
	 */
	const FExMusicPhasePreset* FindPreset(FGameplayTag PhaseTag) const
	{
		for (const FExMusicPhasePreset& Preset : PhasePresets)
		{
			if (Preset.PhaseTag == PhaseTag)
			{
				return &Preset;
			}
		}
		return nullptr;
	}
};
