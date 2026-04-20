// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ExPresetDataAsset.generated.h"

/**
 * UExPresetDataAsset — Preset(프리셋형) DataAsset의 추상 베이스 클래스.
 *
 * 역할:
 *   Definition들의 조합과 룰(이번 판은 이렇게 플레이한다)을 담는 DataAsset의 최상위 부모이다.
 *   모드·난이도별로 M개의 인스턴스가 존재한다.
 *
 * Config와의 결정적 차이:
 *   - Config는 "속도 600, 확률 0.5" 같은 수치 자체 (프로젝트에 하나)
 *   - Preset은 "이 Definition들을 이 조건으로 조합한다"는 구성(모드별 여러 개)
 *
 * 분류 기준:
 *   "이 데이터는 다른 Definition들을 조합하고 조건/확률/순서를 정의하는가?" → Yes → Preset 계열 상속
 *
 * DataCenter 접근:
 *   UExDataCenterSubsystem::GetPreset<UExRunnerRulePreset>(Ex.Mode.Endless 태그)
 *
 * 저장소 구조:
 *   PresetMap은 UClass*를 상위 키로, FGameplayTag를 하위 키로 사용한다.
 *   동일 모드 태그를 서로 다른 Preset 타입(RulePreset, SpawnPreset)이 공유해도
 *   타입 공간이 분리되어 있으므로 덮어쓰기 충돌이 발생하지 않는다.
 */
UCLASS(Abstract, BlueprintType)
class EXCORERUNTIME_API UExPresetDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/**
	 * 이 Preset이 적용되는 모드/난이도 식별 태그.
	 * DataCenter에서 GetPreset<T>(ModeTag)로 조회할 때 사용된다.
	 * 예: Ex.Mode.Endless, Ex.Difficulty.Easy
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset|Identity")
	FGameplayTag PresetTag;

	/** PrimaryAssetType 반환 — Asset Audit에서 "ExPreset" 필터로 조망 가능 */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(FPrimaryAssetType("ExPreset"), GetFName());
	}

#if WITH_EDITOR
	/**
	 * 저장/패키징 시점에 호출되는 데이터 유효성 검증.
	 * - PresetTag가 비어 있으면 에러
	 * 서브클래스에서 Definition 배열 nullptr 체크, 가중치 합계 검증 등을 추가한다.
	 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
