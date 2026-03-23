// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ExItemDefinition.generated.h"

class AExItemPickupBase;
class UExItemEffect;
class USoundBase;
class UNiagaraSystem;

/**
 * 아이템 메타데이터를 담는 DataAsset — 단일 진실 공급원(SSOT).
 *
 * 하나의 아이템에 대한 식별 정보, 비주얼 클래스 참조, 이펙트 로직,
 * 획득 피드백(사운드/VFX)을 모두 이 DataAsset 하나에서 관리한다.
 * 코드 수정 없이 DataAsset 인스턴스를 추가하는 것만으로 새 아이템을 생성할 수 있다.
 */
UCLASS(BlueprintType)
class EXCORERUNTIME_API UExItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ── 식별 ──

	/** 아이템 고유 태그 (Ex.Item.Coin, Ex.Item.SpeedBoost 등) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Identity")
	FGameplayTag ItemTag;

	/** 표시 이름 (UI/현지화용) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Identity")
	FText DisplayName;

	// ── 비주얼 ──

	/**
	 * 필드에 스폰될 픽업 액터 BP 클래스.
	 * 디자이너가 메시, 파티클, 애니메이션, 회전 등 모든 비주얼을
	 * 이 BP 내부에서 자유롭게 구성한다.
	 * AExItemPickupBase를 상속한 BP여야 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Visual")
	TSubclassOf<AExItemPickupBase> PickupActorClass;

	// ── 이펙트 ──

	/**
	 * 획득 시 발동되는 이펙트 (Instanced, 인라인 편집).
	 * ★ Stateless 원칙: 이 이펙트 오브젝트는 공유 인스턴스이므로
	 *   런타임 상태를 절대 저장하지 마라.
	 */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Item|Effect")
	TObjectPtr<UExItemEffect> ItemEffect;

	// ── 사운드/VFX ──

	/** 획득 시 재생할 사운드 (Soft Reference로 비동기 로딩) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Feedback")
	TSoftObjectPtr<USoundBase> PickupSound;

	/** 획득 시 재생할 나이아가라 이펙트 (Soft Reference) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Feedback")
	TSoftObjectPtr<UNiagaraSystem> PickupVFX;
};
