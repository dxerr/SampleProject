// Copyright ExFrameWork. All Rights Reserved.

#include "ExItemPickupBase.h"
#include "ExItemSystemTypes.h"
#include "ExItemDefinition.h"
#include "ExItemEffect.h"
#include "ExItemTags.h"
#include "ExGameplayEventSubsystem.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "ExGameplayEventSubsystem.h"
AExItemPickupBase::AExItemPickupBase()
{
	PrimaryActorTick.bCanEverTick = false;

	// 네트워크 복제 활성화
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(true);

	// 루트 씬 컴포넌트
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// 오버랩 감지용 스피어 콜리전
	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->SetupAttachment(SceneRoot);
	CollisionSphere->InitSphereRadius(80.f);
	CollisionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	CollisionSphere->SetGenerateOverlapEvents(true);
}

void AExItemPickupBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AExItemPickupBase, ItemDefinition);
	DOREPLIFETIME(AExItemPickupBase, OwnerSegmentIndex);
	DOREPLIFETIME(AExItemPickupBase, ReplicatedServerWorldLocation);
	DOREPLIFETIME(AExItemPickupBase, ReplicatedServerWorldRotation);
}

// ── 오버랩 처리 (클라이언트 예측 + 서버 권한) ──

void AExItemPickupBase::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || !ItemDefinition)
	{
		return;
	}

	if (PickupMethod != EExPickupMethod::Overlap)
	{
		return;
	}

	// Pawn 확인
	APawn* OtherPawn = Cast<APawn>(OtherActor);
	if (!OtherPawn)
	{
		return;
	}

	// ── 클라이언트 사전 예측 ──
	// 로컬 플레이어가 오버랩한 경우, 서버 응답을 기다리지 않고 즉시 비주얼 처리
	if (OtherPawn->IsLocallyControlled() && !HasAuthority())
	{
		bLocallyPredicted = true;
		BP_OnPickedUpFeedback(OtherActor);
		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);
	}

	// ── 서버 권한 처리 ──
	if (HasAuthority())
	{
		ServerPickUp(OtherActor);
	}
}

// ── 서버 전용: 아이템 획득 처리 ──

void AExItemPickupBase::ServerPickUp(AActor* PickupInstigator)
{
	// Assert 검증 — Silent Failure 방지
	if (!ensureAlwaysMsgf(ItemDefinition,
		TEXT("[ExItemPickup] %s: ItemDefinition이 할당되지 않았습니다!"), *GetName()))
	{
		return;
	}

	if (!ensureAlwaysMsgf(ItemDefinition->ItemEffect,
		TEXT("[ExItemPickup] %s: ItemEffect가 null입니다! Definition: %s"),
		*GetName(), *ItemDefinition->GetName()))
	{
		return;
	}

	// 이펙트 실행 (서버 Only)
	ItemDefinition->ItemEffect->Execute(PickupInstigator, ItemDefinition, this);

	// 이벤트 브로드캐스트
	if (UWorld* World = GetWorld())
	{
		if (UExGameplayEventSubsystem* EventSub = World->GetSubsystem<UExGameplayEventSubsystem>())
		{
			EventSub->BroadcastEventSimple(ItemDefinition->DefinitionTag, PickupInstigator);
		}
	}

	// 모든 클라이언트에 피드백 전파
	Multicast_OnPickedUp(PickupInstigator);

	// 풀 반환 요청
	OnItemConsumed.Broadcast(this);

	UE_LOG(LogExItemSystem, Log, TEXT("[ExItemPickup] %s가 %s 획득"),
		PickupInstigator ? *PickupInstigator->GetName() : TEXT("Unknown"),
		*ItemDefinition->DisplayName.ToString());
}

// ── Multicast RPC: 중복 방지 ──

void AExItemPickupBase::Multicast_OnPickedUp_Implementation(AActor* PickupInstigator)
{
	// 클라이언트 사전 예측으로 이미 처리한 경우 중복 실행 방지
	if (bLocallyPredicted)
	{
		bLocallyPredicted = false; // 다음 활성화를 위해 리셋
		return;
	}

	// 다른 클라이언트들 (Simulated Proxy) → 정상 피드백 실행
	BP_OnPickedUpFeedback(PickupInstigator);
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

// ── ItemDefinition 복제 수신 ──

void AExItemPickupBase::OnRep_ItemDefinition()
{
	if (ItemDefinition)
	{
		BP_OnActivated();
	}
}

// ── OwnerSegmentIndex 복제 수신 (Phase 4 클라이언트 수동 Attach) ──

void AExItemPickupBase::OnRep_OwnerSegmentIndex()
{
	if (HasAuthority()) return;

	if (UWorld* World = GetWorld())
	{
		if (UExGameplayEventSubsystem* EventSub = World->GetSubsystem<UExGameplayEventSubsystem>())
		{
			// ExCore 모듈은 ExRunnerPlay 모듈에 의존할 수 없으므로, 이벤트를 통해 매니저에게 Attach를 요청한다.
			FGameplayTag AttachTag = FGameplayTag::RequestGameplayTag(FName("Event.Sync.ItemReAttach"), false);
			if (AttachTag.IsValid())
			{
				FExGameplayEventPayload Payload;
				Payload.Instigator = this;
				Payload.OptionalValue = (float)OwnerSegmentIndex;
				EventSub->BroadcastEvent(AttachTag, Payload);
			}
		}
	}
}

// ── 풀링 인터페이스 ──

void AExItemPickupBase::ActivatePickup(const UExItemDefinition* NewDefinition)
{
	ItemDefinition = NewDefinition;

	// 콜리전 이벤트 바인딩 (최초 활성화 시 1회)
	if (!CollisionSphere->OnComponentBeginOverlap.IsBound())
	{
		CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &AExItemPickupBase::OnOverlapBegin);
	}

	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	bLocallyPredicted = false;

	// 서버에서 직접 비주얼 초기화 호출 (클라이언트는 OnRep_ItemDefinition에서 호출)
	if (HasAuthority())
	{
		BP_OnActivated();
	}
}

void AExItemPickupBase::DeactivatePickup()
{
	BP_OnDeactivated();

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	bLocallyPredicted = false;

	// 청크에서 분리
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
}
