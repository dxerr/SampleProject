// Copyright ExFrameWork. All Rights Reserved.

#include "ExItemSpawnManagerBase.h"
#include "ExItemPickupBase.h"
#include "ExItemDefinition.h"
#include "ExItemSystemTypes.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY(LogExItemSystem);

UExItemSpawnManagerBase::UExItemSpawnManagerBase()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UExItemSpawnManagerBase::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogExItemSystem, Log, TEXT("[ExItemSpawnManager] BeginPlay — 풀링 %s, 초기 풀 크기: %d"),
		bUsePooling ? TEXT("활성화") : TEXT("비활성화"), InitialPoolSizePerClass);
}

void UExItemSpawnManagerBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 풀 정리 (액터 파괴는 레벨 전환 시 엔진이 처리)
	ItemPool.Empty();
	Super::EndPlay(EndPlayReason);
}

// ── 스폰 인터페이스 ──

AExItemPickupBase* UExItemSpawnManagerBase::SpawnItem(const UExItemDefinition* Definition, const FTransform& SpawnTransform)
{
	if (!ensureAlwaysMsgf(Definition, TEXT("[ExItemSpawnManager] SpawnItem: Definition이 null입니다!")))
	{
		return nullptr;
	}

	if (!ensureAlwaysMsgf(Definition->PickupActorClass, TEXT("[ExItemSpawnManager] SpawnItem: PickupActorClass가 null입니다! Definition: %s"), *Definition->GetName()))
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	AExItemPickupBase* Item = nullptr;

	// 풀링 시도
	if (bUsePooling)
	{
		Item = GetFromPool(Definition->PickupActorClass);
	}

	// 풀에 없으면 새로 생성
	if (!Item)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		Item = World->SpawnActor<AExItemPickupBase>(Definition->PickupActorClass, SpawnTransform, Params);
		if (!Item)
		{
			UE_LOG(LogExItemSystem, Error, TEXT("[ExItemSpawnManager] SpawnActor 실패: %s"), *Definition->PickupActorClass->GetName());
			return nullptr;
		}

		// 획득(소모) 델리게이트 바인딩
		Item->OnItemConsumed.AddDynamic(this, &UExItemSpawnManagerBase::OnItemConsumed);
	}
	else
	{
		// 풀에서 꺼낸 아이템의 위치 설정
		Item->SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::ResetPhysics);
	}

	// 활성화
	Item->ActivatePickup(Definition);

	return Item;
}

void UExItemSpawnManagerBase::ReturnItemToPool(AExItemPickupBase* Item)
{
	if (!Item)
	{
		return;
	}

	if (bUsePooling)
	{
		DeactivateItem(Item);
		UClass* ItemClass = Item->GetClass();
		TArray<AExItemPickupBase*>& Pool = ItemPool.FindOrAdd(ItemClass);
		Pool.Add(Item);
	}
	else
	{
		// 풀링을 사용하지 않는 경우, 액터를 즉시 파괴하여 언리얼 엔진의 가비지 컬렉터에 위임
		// Blueprint 내 컴포넌트(RotatingMovement, EventGraph 등)의 깔끔한 초기 상태 지향
		Item->Destroy();
	}
}

// ── 내부 풀 관리 ──

AExItemPickupBase* UExItemSpawnManagerBase::GetFromPool(TSubclassOf<AExItemPickupBase> ActorClass)
{
	if (!ActorClass)
	{
		return nullptr;
	}

	TArray<AExItemPickupBase*>* Pool = ItemPool.Find(ActorClass);
	if (Pool && Pool->Num() > 0)
	{
		// FIFO: 먼저 반환된 액터부터 재사용
		AExItemPickupBase* Item = (*Pool)[0];
		Pool->RemoveAt(0);
		return Item;
	}

	return nullptr;
}

void UExItemSpawnManagerBase::ActivateItem(AExItemPickupBase* Item)
{
	if (Item)
	{
		Item->SetActorHiddenInGame(false);
		Item->SetActorEnableCollision(true);
	}
}

void UExItemSpawnManagerBase::DeactivateItem(AExItemPickupBase* Item)
{
	if (Item)
	{
		Item->DeactivatePickup();
	}
}

void UExItemSpawnManagerBase::OnItemConsumed(AExItemPickupBase* Item)
{
	ReturnItemToPool(Item);
}
