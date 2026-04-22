// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ExVisualOverrideComponent.generated.h"

USTRUCT(BlueprintType)
struct FVisualOverrideState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Visual")
	TSubclassOf<AActor> VisualClass = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Visual")
	bool bHideOwnerMesh = true;

	UPROPERTY(BlueprintReadOnly, Category = "Visual")
	bool bCopyAnimationFromVisual = false;
};

/**
 * UExVisualOverrideComponent
 * 캐릭터의 Visual Override를 관리하는 컴포넌트
 * AC_VisualOverrideManager를 참고하여 C++ 구현
 */
UCLASS(ClassGroup=(ExCore), meta=(BlueprintSpawnableComponent))
class EXCORERUNTIME_API UExVisualOverrideComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UExVisualOverrideComponent();

	/**
	 * 서버에서 호출하여 모든 클라이언트로 Visual Override 정보를 전송합니다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Visual")
	void SetVisualOverride(TSubclassOf<AActor> VisualClass, bool bHideMesh = true, bool bCopyAnim = false);

	/**
	 * 현재 적용된 Visual Actor
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<AActor> CurrentVisualActor;

	/**
	 * 적용된 Visual 클래스
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
	TSubclassOf<AActor> CurrentVisualClass;

	/**
	 * 네트워크 리플리케이션을 위한 상태 정보
	 */
	UPROPERTY(ReplicatedUsing = OnRep_VisualState)
	FVisualOverrideState VisualState;

	UFUNCTION()
	void OnRep_VisualState();

	/**
	 * Visual Override 로컬 적용 (Client & Server)
	 */
	AActor* ApplyVisualOverrideLocally(const FVisualOverrideState& InState);

	/**
	 * Visual Override 적용
	 * @param VisualClass 적용할 Visual Actor 클래스
	 * @return 생성된 Visual Actor
	 */
	UFUNCTION(BlueprintCallable, Category = "Visual")
	AActor* ApplyVisualOverride(TSubclassOf<AActor> VisualClass);

	/**
	 * 현재 Visual 제거
	 */
	UFUNCTION(BlueprintCallable, Category = "Visual")
	void ClearVisualOverride();

	/**
	 * 컨테이너 메시 표시/숨김 토글
	 */
	UFUNCTION(BlueprintCallable, Category = "Visual")
	void SetOwnerMeshVisibility(bool bVisible);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/**
	 * 오너 캐릭터의 Mesh 컴포넌트 캐시
	 */
	TWeakObjectPtr<USkeletalMeshComponent> CachedOwnerMesh;
};
