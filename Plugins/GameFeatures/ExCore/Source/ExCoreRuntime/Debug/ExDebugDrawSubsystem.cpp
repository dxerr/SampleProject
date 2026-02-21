// Copyright ExFrameWork. All Rights Reserved.

#include "ExDebugDrawSubsystem.h"
#include "ExDebugStateSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY_STATIC(LogExDebugDraw, Log, All);

// ========== 수명주기 ==========

void UExDebugDrawSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogExDebugDraw, Log, TEXT("ExDebugDrawSubsystem 초기화"));
}

TStatId UExDebugDrawSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UExDebugDrawSubsystem, STATGROUP_Tickables);
}

bool UExDebugDrawSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return true;
#endif
}

void UExDebugDrawSubsystem::Tick(float DeltaTime)
{
	// Persistent Draw 처리
	for (auto& Pair : PersistentDraws)
	{
		const FExPersistentDraw& Draw = Pair.Value;
		if (IsCategoryActive(Draw.Category) && Draw.DrawFunc)
		{
			Draw.DrawFunc(GetWorld(), DeltaTime);
		}
	}
}

// ========== 조건부 DrawDebug ==========

void UExDebugDrawSubsystem::DrawLineChecked(FGameplayTag Category, const FVector& Start, const FVector& End,
											FColor Color, float Thickness, float Duration)
{
	if (!IsCategoryActive(Category)) return;
	DrawDebugLine(GetWorld(), Start, End, Color, false, Duration, 0, Thickness);
}

void UExDebugDrawSubsystem::DrawSphereChecked(FGameplayTag Category, const FVector& Center,
											  float Radius, int32 Segments, FColor Color, float Duration)
{
	if (!IsCategoryActive(Category)) return;
	DrawDebugSphere(GetWorld(), Center, Radius, Segments, Color, false, Duration);
}

void UExDebugDrawSubsystem::DrawBoxChecked(FGameplayTag Category, const FVector& Center, const FVector& Extent,
										   FColor Color, float Duration)
{
	if (!IsCategoryActive(Category)) return;
	DrawDebugBox(GetWorld(), Center, Extent, Color, false, Duration);
}

void UExDebugDrawSubsystem::DrawScreenTextChecked(FGameplayTag Category, const FString& Text,
												  FColor Color, float Duration)
{
	if (!IsCategoryActive(Category)) return;
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, Duration, Color, Text);
	}
}

// ========== Persistent Draw ==========

int32 UExDebugDrawSubsystem::RegisterPersistentDraw(FGameplayTag Category, TFunction<void(UWorld*, float)> DrawFunc)
{
	const int32 Id = NextDrawId++;
	FExPersistentDraw Draw;
	Draw.Category = Category;
	Draw.DrawFunc = MoveTemp(DrawFunc);
	PersistentDraws.Add(Id, MoveTemp(Draw));
	UE_LOG(LogExDebugDraw, Log, TEXT("Persistent Draw 등록: ID=%d, Category=%s"), Id, *Category.ToString());
	return Id;
}

void UExDebugDrawSubsystem::UnregisterPersistentDraw(int32 Id)
{
	PersistentDraws.Remove(Id);
}

void UExDebugDrawSubsystem::ClearPersistentDraws(FGameplayTag Category)
{
	for (auto It = PersistentDraws.CreateIterator(); It; ++It)
	{
		if (It->Value.Category == Category)
		{
			It.RemoveCurrent();
		}
	}
}

// ========== 내부 유틸 ==========

bool UExDebugDrawSubsystem::IsCategoryActive(FGameplayTag Category) const
{
	// 캐시된 DebugState가 없으면 동적으로 찾기
	if (!CachedDebugState)
	{
		const UWorld* World = GetWorld();
		if (!World) return false;
		UGameInstance* GI = World->GetGameInstance();
		if (!GI) return false;
		const_cast<UExDebugDrawSubsystem*>(this)->CachedDebugState = GI->GetSubsystem<UExDebugStateSubsystem>();
	}

	if (!CachedDebugState) return false;
	return CachedDebugState->IsCheatEnabled(Category);
}
