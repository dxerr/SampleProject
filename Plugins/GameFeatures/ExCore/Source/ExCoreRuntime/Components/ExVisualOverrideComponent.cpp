// Copyright ExFrameWork. All Rights Reserved.

#include "ExVisualOverrideComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogExVisualOverride, Log, All);

UExVisualOverrideComponent::UExVisualOverrideComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bHideOwnerMesh = true;
}

void UExVisualOverrideComponent::BeginPlay()
{
	Super::BeginPlay();

	// 오너 캐릭터의 Mesh 캐시
	if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		CachedOwnerMesh = OwnerCharacter->GetMesh();
	}
}

void UExVisualOverrideComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearVisualOverride();
	Super::EndPlay(EndPlayReason);
}

AActor* UExVisualOverrideComponent::ApplyVisualOverride(TSubclassOf<AActor> VisualClass)
{
	if (!VisualClass)
	{
		UE_LOG(LogExVisualOverride, Warning, TEXT("ApplyVisualOverride: VisualClass is null"));
		return nullptr;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		UE_LOG(LogExVisualOverride, Error, TEXT("ApplyVisualOverride: Owner is null"));
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// 기존 Visual 제거
	ClearVisualOverride();

	UE_LOG(LogExVisualOverride, Log, TEXT("Applying VisualOverride: %s to %s"), 
		*VisualClass->GetName(), *Owner->GetName());

	// Visual Actor 스폰
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = Owner;

	CurrentVisualActor = World->SpawnActor<AActor>(
		VisualClass,
		Owner->GetActorLocation(),
		Owner->GetActorRotation(),
		SpawnParams
	);

	if (!CurrentVisualActor)
	{
		UE_LOG(LogExVisualOverride, Error, TEXT("Failed to spawn VisualActor"));
		return nullptr;
	}

	CurrentVisualClass = VisualClass;

	// 부착 처리
	if (CachedOwnerMesh.IsValid())
	{
		CurrentVisualActor->AttachToComponent(
			CachedOwnerMesh.Get(),
			FAttachmentTransformRules::SnapToTargetIncludingScale
		);

		// 컨테이너 메시 숨기기
		if (bHideOwnerMesh)
		{
			SetOwnerMeshVisibility(false);
		}
	}
	else if (Owner->GetRootComponent())
	{
		CurrentVisualActor->AttachToComponent(
			Owner->GetRootComponent(),
			FAttachmentTransformRules::SnapToTargetIncludingScale
		);
	}

	UE_LOG(LogExVisualOverride, Log, TEXT("VisualOverride applied: %s"), *CurrentVisualActor->GetName());
	return CurrentVisualActor;
}

void UExVisualOverrideComponent::ClearVisualOverride()
{
	if (CurrentVisualActor)
	{
		CurrentVisualActor->Destroy();
		CurrentVisualActor = nullptr;
	}
	CurrentVisualClass = nullptr;

	// 오너 메시 다시 표시
	SetOwnerMeshVisibility(true);
}

void UExVisualOverrideComponent::SetOwnerMeshVisibility(bool bVisible)
{
	if (CachedOwnerMesh.IsValid())
	{
		CachedOwnerMesh->SetHiddenInGame(!bVisible);
		CachedOwnerMesh->SetVisibility(bVisible);
	}
}
