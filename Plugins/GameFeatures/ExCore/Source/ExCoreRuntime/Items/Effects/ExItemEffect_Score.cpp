// Copyright ExFrameWork. All Rights Reserved.

#include "ExItemEffect_Score.h"
#include "ExItemSystemTypes.h"
#include "ExItemTags.h"
#include "ExPlayerStateBase.h"
#include "ExGameplayEventSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

void UExItemEffect_Score::Execute_Implementation(AActor* Instigator, const UExItemDefinition* ItemDefinition, AExItemPickupBase* ItemActor)
{
	if (!Instigator)
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(Instigator);
	if (!Pawn)
	{
		return;
	}

	// PlayerState 획득 및 타입 검사 (assert 대신 일반 분기문 사용)
	AExPlayerStateBase* PS = Pawn->GetPlayerState<AExPlayerStateBase>();
	if (!PS)
	{
		UE_LOG(LogExItemSystem, Warning, TEXT("[ExItemEffect_Score] %s의 PlayerState를 AExPlayerStateBase로 캐스팅할 수 없습니다. 점수를 추가하지 못했습니다."), *Instigator->GetName());
		return;
	}

	PS->AddScore(ScoreAmount);

	// 이벤트 브로드캐스트 (UI/사운드 구독용)
	if (UWorld* World = Instigator->GetWorld())
	{
		if (UExGameplayEventSubsystem* EventSub = World->GetSubsystem<UExGameplayEventSubsystem>())
		{
			EventSub->BroadcastEventSimple(TAG_Ex_Item_PickedUp_Score, Instigator);
		}
	}

	UE_LOG(LogExItemSystem, Verbose, TEXT("[ExItemEffect_Score] %s에게 %.0f점 추가"), *Instigator->GetName(), ScoreAmount);
}

FText UExItemEffect_Score::GetEffectDescription_Implementation() const
{
	return FText::Format(
		NSLOCTEXT("ExItemEffect", "ScoreDesc", "점수 +{0}"),
		FText::AsNumber(static_cast<int32>(ScoreAmount))
	);
}
