// Fill out your copyright notice in the Description page of Project Settings.

#include "Util/Actor/ExActorUtil.h"
#include "Components/ActorComponent.h"

APawn* UExActorUtil::FindOwnerPawn(AActor* InActor)
{
	if (!InActor) return nullptr;

	// 1순위: InActor 자신이 Pawn인 경우
	if (APawn* AsPawn = Cast<APawn>(InActor))
	{
		return AsPawn;
	}

	// 2순위: Owner가 Pawn인 경우
	if (APawn* OwnerPawn = Cast<APawn>(InActor->GetOwner()))
	{
		return OwnerPawn;
	}

	// 3순위: AttachParent가 Pawn인 경우
	//        (시각용 SkeletalMesh Actor가 Pawn의 자식으로 Attach된 구조 대응)
	if (APawn* AttachParentPawn = Cast<APawn>(InActor->GetAttachParentActor()))
	{
		return AttachParentPawn;
	}

	return nullptr;
}

APawn* UExActorUtil::FindOwnerPawn(UActorComponent* InComponent)
{
	if (!InComponent) return nullptr;
	return FindOwnerPawn(InComponent->GetOwner());
}
