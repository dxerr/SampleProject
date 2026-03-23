// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ExItemSpawnManagerBase.generated.h"

class AExItemPickupBase;
class UExItemDefinition;

/**
 * 아이템 스폰 매니저 베이스 클래스 (ActorComponent).
 *
 * ExObstacleManager와 동일한 FIFO 풀링 패턴을 적용한다.
 * - 풀 반환: DetachFromActor → 숨김 → 충돌 비활성화
 * - 풀 재사용: FIFO 순서로 꺼내어 비주얼 초기화 → 노출 → 충돌 활성화
 *
 * Feature 모듈(ExRunnerPlay 등)에서 이 클래스를 상속받아
 * 전용 배치 로직(청크 연동, Z축 결정 등)을 구현한다.
 */
UCLASS(Abstract, ClassGroup = (ExCore), meta = (BlueprintSpawnableComponent))
class EXCORERUNTIME_API UExItemSpawnManagerBase : public UActorComponent
{
	GENERATED_BODY()

public:
	UExItemSpawnManagerBase();

	// ── 풀링 설정 ──

	/** 오브젝트 풀링 사용 여부 (디자이너의 BP 초기 설정 보장을 위해 기본 비활성화) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Pool")
	bool bUsePooling = false;

	/** 클래스별 초기 풀 크기 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Pool", meta = (EditCondition = "bUsePooling", ClampMin = "1"))
	int32 InitialPoolSizePerClass = 5;

	// ── 스폰 인터페이스 ──

	/**
	 * 아이템을 스폰(또는 풀에서 꺼내어 활성화)한다.
	 * 서버에서만 호출해야 한다.
	 * @param Definition 스폰할 아이템 정의
	 * @param SpawnTransform 월드 트랜스폼
	 * @return 스폰된(또는 풀에서 꺼낸) 액터
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Item|Spawn")
	AExItemPickupBase* SpawnItem(const UExItemDefinition* Definition, const FTransform& SpawnTransform);

	/**
	 * 아이템을 풀로 반환한다.
	 * @param Item 반환할 아이템 액터
	 */
	UFUNCTION(BlueprintCallable, Category = "Item|Spawn")
	void ReturnItemToPool(AExItemPickupBase* Item);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ── 내부 풀 관리 (FIFO 패턴) ──

	/** 클래스별 아이템 액터 풀 */
	TMap<UClass*, TArray<AExItemPickupBase*>> ItemPool;

	/** 풀에서 사용 가능한 액터를 꺼낸다 (FIFO) */
	AExItemPickupBase* GetFromPool(TSubclassOf<AExItemPickupBase> ActorClass);

	/** 아이템을 활성화 상태로 전환한다 */
	void ActivateItem(AExItemPickupBase* Item);

	/** 아이템을 비활성화 상태로 전환한다 (풀 반환) */
	void DeactivateItem(AExItemPickupBase* Item);

	/** 아이템이 획득(소모)되었을 때 풀 반환 콜백 */
	UFUNCTION()
	void OnItemConsumed(AExItemPickupBase* Item);
};
