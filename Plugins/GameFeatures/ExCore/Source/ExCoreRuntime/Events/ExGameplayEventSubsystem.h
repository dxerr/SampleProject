// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "ExGameplayEventSubsystem.generated.h"

/**
 * GameplayTag 이벤트 페이로드 구조체
 * 이벤트 발행 시 전달할 추가 데이터
 */
USTRUCT(BlueprintType)
struct EXCORERUNTIME_API FExGameplayEventPayload
{
	GENERATED_BODY()

	/** 이벤트를 발생시킨 주체 */
	UPROPERTY(BlueprintReadWrite, Category = "Ex|Event")
	TObjectPtr<UObject> Instigator = nullptr;

	/** 이벤트 대상 (선택적) */
	UPROPERTY(BlueprintReadWrite, Category = "Ex|Event")
	TObjectPtr<AActor> Target = nullptr;

	/** 추가 데이터 (선택적) */
	UPROPERTY(BlueprintReadWrite, Category = "Ex|Event")
	float OptionalValue = 0.0f;

	/** 버프/효과 지속 시간 (선택적, 초) */
	UPROPERTY(BlueprintReadWrite, Category = "Ex|Event")
	float Duration = 0.0f;

	/**
	 * 이 버프 활성화 시 먼저 제거할 버프 태그 목록 (RemoveList).
	 * Ex.Buff.SpeedDown 등 제거 대상 태그를 여기에 낙습니다.
	 * ExItemEffect_Buff 데이터 에셋에서 에디터로 설정하고, Execute에서 페이로드에 포함됩니다.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Ex|Event")
	TArray<FGameplayTag> RemoveList;
};

/** 이벤트 델리게이트 정의 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FExGameplayEventDelegate, FGameplayTag, EventTag, const FExGameplayEventPayload&, Payload);

/**
 * UExGameplayEventSubsystem
 * GameplayTag 기반 이벤트 발행/구독 시스템 (Lyra GameplayMessageRouter 패턴)
 * 
 * 사용법:
 * 1. C++: GetWorld()->GetSubsystem<UExGameplayEventSubsystem>()->BroadcastEvent(TAG_Ex_Action_Climb_Start);
 * 2. BP: Get ExGameplayEventSubsystem -> Broadcast Event 노드
 */
UCLASS()
class EXCORERUNTIME_API UExGameplayEventSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * 이벤트 발행 (C++ 및 BP 모두 사용 가능)
	 * @param EventTag 발행할 이벤트 태그
	 * @param Payload 이벤트 데이터 (선택적)
	 */
	UFUNCTION(BlueprintCallable, Category = "Ex|Events", meta = (DisplayName = "Broadcast Event"))
	void BroadcastEvent(FGameplayTag EventTag, const FExGameplayEventPayload& Payload);

	/**
	 * 이벤트 발행 (간편 버전 - Instigator만 전달)
	 * @param EventTag 발행할 이벤트 태그
	 * @param Instigator 이벤트 발생 주체
	 */
	UFUNCTION(BlueprintCallable, Category = "Ex|Events", meta = (DisplayName = "Broadcast Event Simple"))
	void BroadcastEventSimple(FGameplayTag EventTag, UObject* Instigator = nullptr);

	/**
	 * 이벤트 리스너 등록 (C++ 전용)
	 * @param EventTag 구독할 이벤트 태그
	 * @return 등록된 델리게이트 참조 (AddDynamic 바인딩용)
	 */
	FExGameplayEventDelegate& GetEventDelegate(FGameplayTag EventTag);

	/**
	 * BP용 이벤트 리스너 - 모든 이벤트를 수신
	 * BP에서 이벤트 태그를 필터링하여 처리
	 */
	UPROPERTY(BlueprintAssignable, Category = "Ex|Events")
	FExGameplayEventDelegate OnGameplayEvent;

	/**
	 * 특정 태그의 리스너가 있는지 확인
	 */
	UFUNCTION(BlueprintPure, Category = "Ex|Events")
	bool HasListeners(FGameplayTag EventTag) const;

protected:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

private:
	/** 태그별 델리게이트 맵 */
	UPROPERTY()
	TMap<FGameplayTag, FExGameplayEventDelegate> EventDelegates;
};
