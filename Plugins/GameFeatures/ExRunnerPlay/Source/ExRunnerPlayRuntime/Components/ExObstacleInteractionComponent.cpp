// Copyright ExFrameWork. All Rights Reserved.

#include "ExObstacleInteractionComponent.h"
#include "ExRunnerMovementComponent.h"
#include "GameFramework/Actor.h"

UExObstacleInteractionComponent::UExObstacleInteractionComponent()
{
	// 기본 설정
	PrimaryComponentTick.bCanEverTick = false;
	
	// BoxComponent 기본 설정
	SetCollisionProfileName(TEXT("Trigger"));
	
	// Overlap 이벤트 바인딩
	OnComponentBeginOverlap.AddDynamic(this, &UExObstacleInteractionComponent::OnOverlapBegin);
	OnComponentEndOverlap.AddDynamic(this, &UExObstacleInteractionComponent::OnOverlapEnd);

    // 기본 크기 (나중에 Spawner나 BP에서 조정됨)
    InitBoxExtent(FVector(50.f));
}

void UExObstacleInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UExObstacleInteractionComponent::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == GetOwner()) return;

	// 러너 무브먼트 찾기
 	UExRunnerMovementComponent* RunnerMov = OtherActor->FindComponentByClass<UExRunnerMovementComponent>();
	
	if (RunnerMov)
	{
		// Warp Target Component 찾기
        USceneComponent* TargetComp = nullptr;
        
        // Owner 액터에서 이름으로 검색
        AActor* Owner = GetOwner();
        if (Owner)
        {
            // 1. 이름으로 검색 (ClimbPoint 등)
            TArray<USceneComponent*> Components;
            Owner->GetComponents(Components);
            
            for (USceneComponent* Comp : Components)
            {
                if (Comp->ComponentTags.Contains(WarpTargetName) || Comp->GetName() == WarpTargetName.ToString())
                {
                    TargetComp = Comp;
                    break;
                }
            }
            
            // 2. 없으면 RootComponent 사용
            if (!TargetComp)
            {
                TargetComp = Owner->GetRootComponent();
            }
        }

		RunnerMov->SetInteractionTarget(TargetComp, WarpTargetName);
		
		// UE_LOG(LogTemp, Log, TEXT("Obstacle Interaction START: %s (Target: %s)"), *GetOwner()->GetName(), *WarpTargetName.ToString());
	}
}

void UExObstacleInteractionComponent::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor || OtherActor == GetOwner()) return;

	UExRunnerMovementComponent* RunnerMov = OtherActor->FindComponentByClass<UExRunnerMovementComponent>();
	// [Revert: Premature Overlap Fix]
	// Treadmill Pause 전략을 사용하므로, 장애물이 도망가지 않아 정상적으로 Overlap이 종료될 것임.
	// 따라서 기존 로직대로 overlap end 시 타겟을 해제함.
	if (RunnerMov)
	{
		RunnerMov->ClearInteractionTarget();
		// UE_LOG(LogTemp, Log, TEXT("Obstacle Interaction END: %s"), *GetOwner()->GetName());
	}
}
