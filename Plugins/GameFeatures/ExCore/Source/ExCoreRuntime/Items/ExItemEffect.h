// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ExItemEffect.generated.h"

class UExItemDefinition;
class AExItemPickupBase;

/**
 * 아이템 이펙트의 추상 베이스 클래스.
 *
 * ★★★ STATELESS 원칙 ★★★
 * 이 클래스의 인스턴스는 UExItemDefinition(DataAsset) 내부에서 
 * Instanced UObject로 생성되며, 여러 아이템 액터가 동일 인스턴스를 공유할 수 있다.
 * 따라서 런타임 상태(AActor* 캐싱, 누적값, 타이머 등)를 멤버 변수로 
 * 저장하면 모든 참조자가 값을 공유하여 치명적 버그가 발생한다.
 *
 * 허용: EditAnywhere 설정값 (에디터에서 설정, 런타임 불변)
 * 금지: 런타임에 변경되는 어떠한 멤버 변수
 *
 * 버프 지속 시간, 스택 카운트 등 상태 관리는 반드시 
 * Instigator의 컴포넌트(StatComp, MovementComp 등)에 위임하라.
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class EXCORERUNTIME_API UExItemEffect : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 아이템 획득 시 서버에서 호출되는 핵심 함수.
	 * ★ 이 함수 내에서 this의 멤버 변수에 런타임 값을 쓰지 마라.
	 * @param Instigator 아이템을 획득한 액터 (보통 Pawn)
	 * @param ItemDefinition 발동 원인이 된 아이템 정의
	 * @param ItemActor 필드에 있던 픽업 액터 (위치 정보 등 참조용)
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Item|Effect")
	void Execute(AActor* Instigator, const UExItemDefinition* ItemDefinition, AExItemPickupBase* ItemActor);
	virtual void Execute_Implementation(AActor* Instigator, const UExItemDefinition* ItemDefinition, AExItemPickupBase* ItemActor);

	/**
	 * 이펙트 설명 텍스트를 반환한다. (UI 툴팁용)
	 * @return 이펙트에 대한 설명 텍스트
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Item|Effect")
	FText GetEffectDescription() const;
	virtual FText GetEffectDescription_Implementation() const;
};
