// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ExRunnerPositionSyncComponent.generated.h"

/**
 * 멀티플레이 환경에서 서버 위치 시각화를 담당하는 디버깅 컴포넌트입니다.
 * 캐릭터 블루프린트에 부착 시 자동으로 동작하며, 
 * 서버 위치를 리플리케이트하여 클라이언트가 캡슐과 텍스트로 시각화합니다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class EXRUNNERPLAYRUNTIME_API UExRunnerPositionSyncComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UExRunnerPositionSyncComponent();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** 서버에서 알고 있는 권위 위치 (클라이언트로 브로드캐스트) */
	UPROPERTY(Replicated)
	FVector ServerAuthLocation;
};
