/**
 * @file ExFeatureAssetManifest.h
 * @brief GameFeature 플러그인 에셋 매니페스트 공통 베이스 클래스
 * @details 각 GameFeature 플러그인이 이 클래스를 상속받아 자신만의 매니페스트를 만들면,
 *          플러그인 활성화 시 해당 에셋들이 자동으로 패키징(쿠킹)에 포함됩니다.
 *
 * [사용법]
 *   1. 각 플러그인 모듈에서 이 클래스를 상속받는 서브클래스를 만듭니다.
 *      예) UExRunnerAssetManifest : public UExFeatureAssetManifest
 *   2. 해당 플러그인의 GameFeatureData 에셋 > Primary Asset Types to Scan 에
 *      서브클래스를 Asset Base Class로 등록하고 Cook Rule을 AlwaysCook으로 설정합니다.
 *   3. 에디터에서 서브클래스를 기반으로 Data Asset을 생성하고, FeatureMaps에 맵을 추가합니다.
 *
 * Copyright ExFrameWork. All Rights Reserved.
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ExFeatureAssetManifest.generated.h"

/**
 * UExFeatureAssetManifest
 *
 * 모든 GameFeature 플러그인의 에셋 매니페스트 공통 베이스.
 * Abstract 클래스이므로 반드시 플러그인별 서브클래스를 만들어 사용해야 합니다.
 */
UCLASS(BlueprintType, Abstract)
class EXCORERUNTIME_API UExFeatureAssetManifest : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * 이 플러그인이 활성화될 때 패키징(쿠킹)에 포함시킬 맵(.umap) 파일 목록.
	 * TSoftObjectPtr<UWorld>를 사용하므로 에디터에서 맵 파일만 선택 가능합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feature|Maps",
		meta = (DisplayName = "맵 목록"))
	TArray<TSoftObjectPtr<UWorld>> FeatureMaps;

	/**
	 * 맵 외에 추가로 강제 포함시킬 범용 에셋 경로 목록.
	 * 데이터테이블, 사운드 큐, 머티리얼 등 어떤 에셋이든 경로로 등록 가능합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feature|Assets",
		meta = (DisplayName = "추가 에셋 목록"))
	TArray<FSoftObjectPath> AdditionalAssets;

	/**
	 * 에디터 표시용 설명 필드.
	 * 이 매니페스트가 어떤 플러그인/기능용인지 메모해 두면 유지보수에 도움이 됩니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feature|Info",
		meta = (DisplayName = "설명", MultiLine = true))
	FText ManifestDescription;
};
