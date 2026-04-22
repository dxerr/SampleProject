// Copyright ExFrameWork. All Rights Reserved.

#include "ExVisualOverrideComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Util/Actor/ExActorUtil.h"

DEFINE_LOG_CATEGORY_STATIC(LogExVisualOverride, Log, All);

UExVisualOverrideComponent::UExVisualOverrideComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	
	// 컴포넌트 리플리케이션 활성화
	SetIsReplicatedByDefault(true);
}

void UExVisualOverrideComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UExVisualOverrideComponent, VisualState);
}

void UExVisualOverrideComponent::SetVisualOverride(TSubclassOf<AActor> VisualClass, bool bHideMesh, bool bCopyAnim)
{
	if (GetOwnerRole() == ROLE_Authority)
	{
		VisualState.VisualClass = VisualClass;
		VisualState.bHideOwnerMesh = bHideMesh;
		VisualState.bCopyAnimationFromVisual = bCopyAnim;
		
		// 서버도 로컬 적용을 위해 명시적으로 호출
		OnRep_VisualState();
	}
}

void UExVisualOverrideComponent::OnRep_VisualState()
{
	ApplyVisualOverrideLocally(VisualState);
}

void UExVisualOverrideComponent::BeginPlay()
{
	Super::BeginPlay();

	// 오너 캐릭터의 Mesh 캐시 (ACharacter 지원 및 커스텀 계층 구조 지원)
	AActor* Owner = GetOwner();
	if (Owner)
	{
		if (ACharacter* OwnerCharacter = Cast<ACharacter>(Owner))
		{
			CachedOwnerMesh = OwnerCharacter->GetMesh();
		}
		
		if (!CachedOwnerMesh.IsValid())
		{
			CachedOwnerMesh = UExActorUtil::FindComponentInHierarchy<USkeletalMeshComponent>(this);
		}
	}
}

void UExVisualOverrideComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearVisualOverride();
	Super::EndPlay(EndPlayReason);
}

AActor* UExVisualOverrideComponent::ApplyVisualOverride(TSubclassOf<AActor> VisualClass)
{
	// 명시적 호출 시 호환성을 위해 리플리케이트되지 않는 기존 로직(로컬 상태)만 임시 세팅
	FVisualOverrideState TempState;
	TempState.VisualClass = VisualClass;
	TempState.bHideOwnerMesh = true;
	return ApplyVisualOverrideLocally(TempState);
}

AActor* UExVisualOverrideComponent::ApplyVisualOverrideLocally(const FVisualOverrideState& InState)
{
	if (!InState.VisualClass)
	{
		ClearVisualOverride();
		return nullptr;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		UE_LOG(LogExVisualOverride, Error, TEXT("ApplyVisualOverrideLocally: Owner is null"));
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// 이미 같은 클래스가 적용 중이라면 무시할 수도 있지만 현재는 다시 갱신
	ClearVisualOverride();

	UE_LOG(LogExVisualOverride, Log, TEXT("Applying VisualOverrideLocally: %s to %s"),
		*InState.VisualClass->GetName(), *Owner->GetName());

	// Visual Actor 스폰 (Cosmetic, Replicates=false 여도 클라이언트가 직접 스폰)
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = Owner;

	CurrentVisualActor = World->SpawnActor<AActor>(
		InState.VisualClass,
		Owner->GetActorLocation(),
		Owner->GetActorRotation(),
		SpawnParams
	);

	if (!CurrentVisualActor)
	{
		UE_LOG(LogExVisualOverride, Error, TEXT("Failed to spawn VisualActor locally"));
		return nullptr;
	}

	CurrentVisualClass = InState.VisualClass;

	// BeginPlay보다 먼저 적용될 경우를 대비해 캐시 보정
	if (!CachedOwnerMesh.IsValid())
	{
		if (ACharacter* OwnerCharacter = Cast<ACharacter>(Owner))
		{
			CachedOwnerMesh = OwnerCharacter->GetMesh();
		}
		if (!CachedOwnerMesh.IsValid())
		{
			CachedOwnerMesh = UExActorUtil::FindComponentInHierarchy<USkeletalMeshComponent>(this);
		}
	}

	// 부착 처리 또는 애니메이션 복사
	if (CachedOwnerMesh.IsValid())
	{
		USkeletalMeshComponent* VisualMesh = CurrentVisualActor->FindComponentByClass<USkeletalMeshComponent>();

		CurrentVisualActor->AttachToComponent(
			CachedOwnerMesh.Get(),
			FAttachmentTransformRules::SnapToTargetIncludingScale
		);

		if (InState.bCopyAnimationFromVisual && VisualMesh)
		{
			// Visual의 정보를 ContainerMesh에 덮어씌움
			USkeletalMesh* VisualSkeletalMesh = VisualMesh->GetSkeletalMeshAsset();
			if (VisualSkeletalMesh)
			{
				CachedOwnerMesh->SetSkeletalMesh(VisualSkeletalMesh, false);
			}

			if (TSubclassOf<UAnimInstance> VisualAnimClass = VisualMesh->GetAnimClass())
			{
				CachedOwnerMesh->SetAnimInstanceClass(VisualAnimClass);
			}

			VisualMesh->SetHiddenInGame(true);
			VisualMesh->SetVisibility(false);

			CachedOwnerMesh->SetHiddenInGame(false);
			CachedOwnerMesh->SetVisibility(true);
		}
		else
		{
			// 컨테이너 메시 숨기기
			if (InState.bHideOwnerMesh)
			{
				SetOwnerMeshVisibility(false);
			}
		}
	}
	else if (Owner->GetRootComponent())
	{
		CurrentVisualActor->AttachToComponent(
			Owner->GetRootComponent(),
			FAttachmentTransformRules::SnapToTargetIncludingScale
		);
	}

	UE_LOG(LogExVisualOverride, Log, TEXT("VisualOverride locally applied: %s"), *CurrentVisualActor->GetName());
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
