// Fill out your copyright notice in the Description page of Project Settings.

#include "Components/ExInventoryComponent.h"

UExInventoryComponent::UExInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	
	// 모바일/일반 환경 최적화를 위해 활성화시에만 틱을 하거나 아예 끄도록 설정
	// 컴포넌트 리플리케이션 옵션 켬 (PlayerState 등에 붙어 배열 동기화 시 필요)
	SetIsReplicatedByDefault(true);
}

void UExInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UExInventoryComponent::Server_UseItem_Implementation(FName ItemID)
{
	// 1. 유효성 검증 (보유 여부, 사용 가능 상태 등)
	if (!ValidateItemUsage(ItemID))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ExInventoryComponent] %s 아이템 사용이 거부되었습니다."), *ItemID.ToString());
		return;
	}

	// 2. 서버 권한으로 아이템 사용 효과 발동
	ApplyItemEffect(ItemID);

	// 3. (향후 추가) 인벤토리에서 실제 아이템 차감 후 리플리케이트 데이터 갱신
}

bool UExInventoryComponent::ValidateItemUsage(FName ItemID) const
{
	// 기본 로직 구조 (재정의 가능). 지금은 임시로 무조건 true 반환.
	if (ItemID.IsNone())
	{
		return false;
	}
	
	return true;
}

void UExInventoryComponent::ApplyItemEffect_Implementation(FName ItemID)
{
	UE_LOG(LogTemp, Log, TEXT("[ExInventoryComponent] %s 아이템 효과가 서버 권한으로 발동되었습니다."), *ItemID.ToString());
}
