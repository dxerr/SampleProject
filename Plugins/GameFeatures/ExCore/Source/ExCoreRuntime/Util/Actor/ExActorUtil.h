// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

/**
 * [ExCore Utility - Actor]
 * Actor 계층 탐색과 관련된 범용 유틸리티 함수 모음입니다.
 * 모든 함수는 static이며 인스턴스 없이 어디서든 호출 가능합니다.
 *
 * 사용 예:
 *   APawn* Pawn = UExActorUtil::FindOwnerPawn(this);
 */
class EXCORERUNTIME_API UExActorUtil
{
public:
	/**
	 * 컴포넌트 또는 Actor의 Owner 계층에서 가장 가까운 Pawn을 탐색합니다.
	 *
	 * 탐색 순서:
	 *   1. InActor 자신이 Pawn인 경우
	 *   2. InActor->GetOwner()가 Pawn인 경우
	 *   3. InActor->GetAttachParentActor()가 Pawn인 경우
	 *      (시각적 SkeletalMesh Actor가 Pawn 자식으로 붙어있는 구조 대응)
	 *
	 * @param InActor  탐색 기준 Actor (컴포넌트의 경우 GetOwner() 결과를 전달)
	 * @return 찾은 APawn 포인터. 없으면 nullptr
	 */
	static APawn* FindOwnerPawn(AActor* InActor);

	/**
	 * 컴포넌트(UActorComponent)의 Owner 계층에서 가장 가까운 Pawn을 탐색합니다.
	 * FindOwnerPawn(AActor*)의 컴포넌트 편의 오버로드입니다.
	 *
	 * @param InComponent  탐색 기준 컴포넌트
	 * @return 찾은 APawn 포인터. 없으면 nullptr
	 */
	static APawn* FindOwnerPawn(UActorComponent* InComponent);
};
