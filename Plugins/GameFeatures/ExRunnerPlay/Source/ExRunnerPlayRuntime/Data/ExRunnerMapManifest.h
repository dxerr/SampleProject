#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ExRunnerMapManifest.generated.h"

/**
 * UExRunnerMapManifest
 * ExRunnerPlay 플러그인에 종속된 맵 파일들을 패키징 시 자동으로 쿠킹(포함)시키기 위한 매니페스트 데이터 에셋.
 */
UCLASS(BlueprintType)
class EXRUNNERPLAYRUNTIME_API UExRunnerMapManifest : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 에디터에서 오직 맵(.umap) 파일만 배열로 선택할 수 있도록 강제 필터링합니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game Feature Maps")
	TArray<TSoftObjectPtr<UWorld>> FeatureMaps;
};
