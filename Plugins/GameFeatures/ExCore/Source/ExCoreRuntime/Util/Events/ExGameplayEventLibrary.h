// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "ExGameplayEventSubsystem.h"
#include "ExGameplayEventLibrary.generated.h"


/**
 * UExGameplayEventLibrary
 * Blueprint에서 GameplayTag 이벤트를 쉽게 발행할 수 있는 헬퍼 함수 라이브러리
 */
UCLASS()
class EXCORERUNTIME_API UExGameplayEventLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * GameplayTag 이벤트 발행 (BP용 정적 함수)
	 * @param WorldContextObject 월드 컨텍스트 (Self 등)
	 * @param EventTag 발행할 이벤트 태그 (Ex.Action.Climb.Start 등)
	 * @param Instigator 이벤트 발생 주체 (선택적)
	 */
	UFUNCTION(BlueprintCallable, Category = "Ex|Events", meta = (WorldContext = "WorldContextObject", DisplayName = "Broadcast Gameplay Event"))
	static void BroadcastGameplayEvent(UObject* WorldContextObject, FGameplayTag EventTag, UObject* Instigator = nullptr);

	/**
	 * ExGameplayEventSubsystem 가져오기
	 * @param WorldContextObject 월드 컨텍스트
	 * @return ExGameplayEventSubsystem 인스턴스 (없으면 nullptr)
	 */
	UFUNCTION(BlueprintPure, Category = "Ex|Events", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Ex Gameplay Event Subsystem"))
	static UExGameplayEventSubsystem* GetExGameplayEventSubsystem(UObject* WorldContextObject);
};
