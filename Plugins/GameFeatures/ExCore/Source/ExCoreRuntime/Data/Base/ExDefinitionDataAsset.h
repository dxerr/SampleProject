// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ExDefinitionDataAsset.generated.h"

/**
 * UExDefinitionDataAsset — Definition(정의형) DataAsset의 추상 베이스 클래스.
 *
 * 역할:
 *   개체 메타데이터(이것은 무엇인가)를 담는 DataAsset의 최상위 부모이다.
 *   아이템, 장애물 등 고유한 비주얼·이펙트·속성을 가진 개체를 정의한다.
 *   종류만큼 N개의 인스턴스를 생성한다.
 *
 * 분류 기준:
 *   "이 데이터는 고유한 비주얼, 이펙트, 속성을 가진 독립 개체를 정의하는가?" → Yes → Definition 계열 상속
 *
 * 태그 유일성 규칙:
 *   DefinitionTag는 동일 클래스 내에서 반드시 유일해야 한다.
 *   IsDataValid에서 중복 등록 시 에러로 차단한다.
 *
 * 캐싱 규칙:
 *   DataCenter에서 조회 후 로컬 캐싱 시 반드시 TWeakObjectPtr<T>를 사용한다.
 *
 * DataCenter 접근:
 *   UExDataCenterSubsystem::FindDefinition<UExItemDefinition>(Tag)
 *   UExDataCenterSubsystem::GetAllDefinitions<UExItemDefinition>()
 */
UCLASS(Abstract, BlueprintType)
class EXCORERUNTIME_API UExDefinitionDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/**
	 * 개체 고유 식별 태그.
	 * 동일 클래스(UClass) 내에서 반드시 유일해야 한다.
	 * 이 태그로 DataCenter에서 검색한다.
	 * 예: Ex.Item.Coin, Ex.Obstacle.Gap
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AssetRegistrySearchable, Category = "Definition|Identity")
	FGameplayTag DefinitionTag;

	/** PrimaryAssetType 반환 — Asset Audit에서 "ExDefinition" 필터로 조망 가능 */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(FPrimaryAssetType("ExDefinition"), GetFName());
	}

#if WITH_EDITOR
	/**
	 * 저장/패키징 시점에 호출되는 데이터 유효성 검증.
	 * - DefinitionTag가 비어 있으면 에러
	 * 서브클래스에서 필수 레퍼런스(nullptr 여부 등)를 추가로 검증한다.
	 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
