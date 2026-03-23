// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ExItemSystemTypes.h"
#include "ExItemPickupBase.generated.h"

class UExItemDefinition;
class USphereComponent;

/**
 * 아이템 픽업 액터 베이스 클래스.
 *
 * - 서버에서만 SpawnActor하며, 기본적으로 클라이언트에 복제된다.
 * - 획득 판정은 서버 오버랩에서만 수행한다.
 * - 클라이언트 예측: 로컬 플레이어(Autonomous Proxy) 오버랩 시
 *   서버 응답을 기다리지 않고 즉시 비주얼 숨김 + 피드백 재생.
 * - 풀링 인터페이스 (ActivatePickup / DeactivatePickup) 제공.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class EXCORERUNTIME_API AExItemPickupBase : public AActor
{
	GENERATED_BODY()

public:
	AExItemPickupBase();

	// ── 데이터 참조 ──

	/**
	 * 이 액터가 나타내는 아이템 정의.
	 * 스폰 매니저가 풀에서 꺼낸 후 할당한다.
	 * Replicated: 클라이언트도 어떤 아이템인지 알아야 피드백(VFX, SFX) 가능.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_ItemDefinition, BlueprintReadOnly, Category = "Item")
	TObjectPtr<const UExItemDefinition> ItemDefinition;

	UFUNCTION()
	void OnRep_ItemDefinition();

	// ── 획득 방식 ──

	/** 현재 획득 방식 (기본: 오버랩 즉시 획득) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Pickup")
	EExPickupMethod PickupMethod = EExPickupMethod::Overlap;

	// ── 서버 전용: 아이템 획득 처리 ──

	/**
	 * 서버에서 호출. 아이템 획득 판정 및 이펙트 실행.
	 * @param PickupInstigator 획득 주체 (Pawn)
	 */
	UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "Item")
	void ServerPickUp(AActor* PickupInstigator);

	// ── BP 확장 포인트 ──

	/** 획득 시 클라이언트 비주얼 피드백 (파티클, 사운드). Authority와 무관하게 로컬에서 호출. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Item|Feedback")
	void BP_OnPickedUpFeedback(AActor* PickupInstigator);

	/** 스폰/활성화 시 비주얼 초기화 (회전 시작, 파티클 활성화 등) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Item|Visual")
	void BP_OnActivated();

	/** 비활성화/풀 반환 시 비주얼 정리 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Item|Visual")
	void BP_OnDeactivated();

	// ── 풀링 인터페이스 (매니저가 호출) ──

	/** 풀에서 꺼내어 활성화. ItemDefinition을 설정하고 비주얼 초기화. */
	void ActivatePickup(const UExItemDefinition* NewDefinition);

	/** 풀로 반환. 비주얼 정리 및 숨김/충돌 비활성화. */
	void DeactivatePickup();

	// ── Replication ──
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ── Multicast RPC ──
	/** 모든 클라이언트에게 획득 피드백 전파 (Unreliable: 피드백 누락은 허용) */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_OnPickedUp(AActor* PickupInstigator);

	// ── 풀 반환 델리게이트 ──

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemConsumed, AExItemPickupBase*, Item);

	UPROPERTY(BlueprintAssignable, Category = "Item")
	FOnItemConsumed OnItemConsumed;

protected:
	/** 오버랩 충돌 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item|Collision")
	TObjectPtr<USphereComponent> CollisionSphere;

	/** 루트 씬 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<USceneComponent> SceneRoot;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

private:
	/**
	 * 클라이언트 예측 중복 방지 플래그.
	 * 로컬에서 이미 예측 처리한 경우, Multicast 도착 시 피드백 중복 실행을 방지.
	 */
	bool bLocallyPredicted = false;
};
