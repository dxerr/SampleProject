// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ExInventoryComponent.generated.h"

/**
 * 부착 대상(Pawn, PlayerController, PlayerState 등)에 독립적으로 설계된 범용 인벤토리 컴포넌트 베이스.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class EXCORERUNTIME_API UExInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UExInventoryComponent();

protected:
	virtual void BeginPlay() override;

public:	
	/**
	 * 클라이언트가 아이템 사용을 서버에 요청합니다.
	 * 서버는 ValidateItemUsage를 통해 검증 후 실제 효과를 적용합니다.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "ExInventory")
	void Server_UseItem(FName ItemID);
	virtual void Server_UseItem_Implementation(FName ItemID);

protected:
	/**
	 * 아이템 사용 유효성 검증 함수 (서버 권한에서만 호출되는 것을 상정)
	 */
	virtual bool ValidateItemUsage(FName ItemID) const;

	/**
	 * 아이템 사용 로직. 하위 클래스나 블루프린트에서 오버라이드하여 기능 구현.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "ExInventory")
	void ApplyItemEffect(FName ItemID);
	virtual void ApplyItemEffect_Implementation(FName ItemID);
};
