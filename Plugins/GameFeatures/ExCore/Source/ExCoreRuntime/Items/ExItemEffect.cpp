// Copyright ExFrameWork. All Rights Reserved.

#include "ExItemEffect.h"
#include "ExItemSystemTypes.h"

void UExItemEffect::Execute_Implementation(AActor* Instigator, const UExItemDefinition* ItemDefinition, AExItemPickupBase* ItemActor)
{
	// 추상 베이스: 서브클래스에서 오버라이드하여 구현
	UE_LOG(LogExItemSystem, Warning, TEXT("[ExItemEffect] Execute_Implementation이 오버라이드되지 않았습니다: %s"), *GetClass()->GetName());
}

FText UExItemEffect::GetEffectDescription_Implementation() const
{
	return FText::GetEmpty();
}
