/**
 * @file ExRunnerAssetManifest.h
 * @brief ExRunnerPlay 플러그인 전용 에셋 매니페스트
 * @details ExCore의 UExFeatureAssetManifest를 상속받아 Runner 전용 에셋을 관리합니다.
 *          맵 목록(FeatureMaps)과 추가 에셋(AdditionalAssets)은 베이스 클래스에 정의되어 있으며,
 *          Runner 플러그인 전용 데이터가 필요하다면 이 클래스에 추가하면 됩니다.
 *
 * Copyright ExFrameWork. All Rights Reserved.
 */

#pragma once

#include "CoreMinimal.h"
#include "ExFeatureAssetManifest.h"
#include "ExRunnerAssetManifest.generated.h"

/**
 * UExRunnerAssetManifest
 * ExRunnerPlay 플러그인 전용 에셋 매니페스트.
 * 맵 및 추가 에셋 등록은 부모 클래스(UExFeatureAssetManifest)의 필드를 사용합니다.
 */
UCLASS(BlueprintType)
class EXRUNNERPLAYRUNTIME_API UExRunnerAssetManifest : public UExFeatureAssetManifest
{
	GENERATED_BODY()

	// 향후 ExRunnerPlay 전용 필드가 필요하면 여기에 추가합니다.
};
