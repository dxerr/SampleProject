// Copyright ExFrameWork. All Rights Reserved.
// ExCore 카테고리별 디버그 시각화 관리 서브시스템

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "ExDebugDrawSubsystem.generated.h"

class UExDebugStateSubsystem;

/**
 * FExPersistentDraw
 * 매 프레임 반복 그리기를 위한 등록 구조체
 */
USTRUCT()
struct FExPersistentDraw
{
	GENERATED_BODY()

	/** 이 그리기가 속한 디버그 카테고리 */
	UPROPERTY()
	FGameplayTag Category;

	/** 그리기 함수 (Lambda로 등록) */
	TFunction<void(UWorld*, float)> DrawFunc;
};

/**
 * UExDebugDrawSubsystem
 * TickableWorldSubsystem — 매 프레임 카테고리별 조건부 디버그 그리기
 * 
 * 역할:
 * - DebugStateSubsystem의 카테고리 활성화 여부를 체크 후 그리기
 * - Persistent(매 프레임) / OneShot(일회성) 그리기 모두 지원
 * - DrawDebugLine, Sphere, Box 등을 카테고리 조건부로 래핑
 * 
 * 사용 예:
 *   auto* DD = GetWorld()->GetSubsystem<UExDebugDrawSubsystem>();
 *   DD->DrawLineChecked(TAG_Ex_Debug_Path, Start, End, FColor::Green);
 */
UCLASS()
class EXCORERUNTIME_API UExDebugDrawSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// ========== 수명주기 ==========
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual TStatId GetStatId() const override;
	virtual void Tick(float DeltaTime) override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// ========== 조건부 DrawDebug (카테고리 체크) ==========

	/** 라인 그리기 (카테고리 활성 시에만) */
	void DrawLineChecked(FGameplayTag Category, const FVector& Start, const FVector& End,
						 FColor Color = FColor::Green, float Thickness = 1.f, float Duration = 0.f);

	/** 구체 그리기 */
	void DrawSphereChecked(FGameplayTag Category, const FVector& Center,
						   float Radius = 20.f, int32 Segments = 12,
						   FColor Color = FColor::Cyan, float Duration = 0.f);

	/** 박스 그리기 */
	void DrawBoxChecked(FGameplayTag Category, const FVector& Center, const FVector& Extent,
						FColor Color = FColor::Yellow, float Duration = 0.f);

	/** 화면 텍스트 (AddOnScreenDebugMessage 래핑) */
	void DrawScreenTextChecked(FGameplayTag Category, const FString& Text,
							   FColor Color = FColor::White, float Duration = 2.f);

	// ========== Persistent Draw 관리 ==========

	/**
	 * 매 프레임 호출될 그리기 함수 등록
	 * @param Category 해당 카테고리가 비활성화되면 자동으로 그리기 중단
	 * @param DrawFunc void(UWorld*, float DeltaTime) 시그니처
	 * @return 등록 ID (해제 시 사용)
	 */
	int32 RegisterPersistentDraw(FGameplayTag Category, TFunction<void(UWorld*, float)> DrawFunc);

	/** 특정 Persistent Draw 해제 */
	void UnregisterPersistentDraw(int32 Id);

	/** 카테고리별 Persistent Draw 전체 해제 */
	void ClearPersistentDraws(FGameplayTag Category);

private:
	/** 카테고리 활성화 여부를 DebugStateSubsystem에서 체크 */
	bool IsCategoryActive(FGameplayTag Category) const;

	/** Persistent Draw 목록 */
	TMap<int32, FExPersistentDraw> PersistentDraws;

	/** 다음 Persistent Draw ID */
	int32 NextDrawId = 0;

	/** DebugStateSubsystem 캐시 (Initialize에서 획득 시도) */
	UPROPERTY()
	TObjectPtr<UExDebugStateSubsystem> CachedDebugState = nullptr;
};
