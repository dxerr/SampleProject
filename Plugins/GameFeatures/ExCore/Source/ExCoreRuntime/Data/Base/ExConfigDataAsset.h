// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ExConfigDataAsset.generated.h"

/**
 * UExConfigDataAsset — Config(설정형) DataAsset의 추상 베이스 클래스.
 *
 * 역할:
 *   전역 수치 튜닝 데이터를 담는 싱글톤 DataAsset의 최상위 부모이다.
 *   INI 파일과 유사한 역할을 하며, 모듈당 1개의 인스턴스만 생성된다.
 *
 * 분류 기준:
 *   "이 값을 바꾸면 게임 전체에 영향을 미치는가?" → Yes → Config 계열 상속
 *
 * 사용법:
 *   이 클래스를 직접 사용하지 않는다. 모듈별 ConfigAsset 클래스를 만들어 상속한다.
 *   예: UExRunnerConfigAsset : public UExConfigDataAsset
 *
 * DataCenter 접근:
 *   UExDataCenterSubsystem::GetConfig<UExRunnerConfigAsset>()
 *
 * 확장 방침:
 *   향후 Config 내 특정 섹션에 다형성이 필요해지면 해당 섹션만 Instanced UObject로 전환한다.
 */
UCLASS(Abstract, BlueprintType)
class EXCORERUNTIME_API UExConfigDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** PrimaryAssetType 반환 — Asset Audit에서 "ExConfig" 필터로 조망 가능 */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(FPrimaryAssetType("ExConfig"), GetFName());
	}

#if WITH_EDITOR
	/**
	 * 저장/패키징 시점에 호출되는 데이터 유효성 검증.
	 * 서브클래스에서 필수 수치 범위(확률 0~1, 거리 양수 등)를 검증한다.
	 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
