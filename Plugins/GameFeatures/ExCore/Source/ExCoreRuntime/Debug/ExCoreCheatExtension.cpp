// Copyright ExFrameWork. All Rights Reserved.

#include "ExCoreCheatExtension.h"
#include "ExDebugStateSubsystem.h"
#include "ExGameplayTags.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogExCoreCheats, Log, All);

// ========== 플레이어 상태 치트 ==========

void UExCoreCheatExtension::ExGodMode()
{
	APlayerController* PC = GetPlayerController();
	if (!PC) return;

	UGameInstance* GI = PC->GetGameInstance();
	if (!GI) return;

	UExDebugStateSubsystem* DS = GI->GetSubsystem<UExDebugStateSubsystem>();
	if (!DS) return;

	const bool bNew = DS->ToggleCheat(TAG_Ex_Debug_GodMode);

	// 실제 God 모드 적용
	if (APawn* Pawn = PC->GetPawn())
	{
		Pawn->SetCanBeDamaged(!bNew);
	}

	UE_LOG(LogExCoreCheats, Log, TEXT("ExGodMode: %s"), bNew ? TEXT("ON — 무적 활성화") : TEXT("OFF — 무적 해제"));
}

void UExCoreCheatExtension::ExSetSpeed(float Speed)
{
	APlayerController* PC = GetPlayerController();
	if (!PC || !PC->GetPawn()) return;

	ACharacter* Character = Cast<ACharacter>(PC->GetPawn());
	if (!Character || !Character->GetCharacterMovement()) return;

	UCharacterMovementComponent* CMC = Character->GetCharacterMovement();

	if (FMath::IsNearlyZero(Speed))
	{
		// 0 = 원래 기본값 복원
		CMC->MaxWalkSpeed = CMC->GetClass()->GetDefaultObject<UCharacterMovementComponent>()->MaxWalkSpeed;
		UE_LOG(LogExCoreCheats, Log, TEXT("ExSetSpeed: 기본값 복원 (%.0f)"), CMC->MaxWalkSpeed);
	}
	else
	{
		CMC->MaxWalkSpeed = Speed;
		UE_LOG(LogExCoreCheats, Log, TEXT("ExSetSpeed: %.0f"), Speed);
	}
}

// ========== 월드 치트 ==========

void UExCoreCheatExtension::ExSlowMo(float TimeDilation)
{
	UWorld* World = GetWorld();
	if (!World) return;

	TimeDilation = FMath::Clamp(TimeDilation, 0.01f, 10.f);
	UGameplayStatics::SetGlobalTimeDilation(World, TimeDilation);
	UE_LOG(LogExCoreCheats, Log, TEXT("ExSlowMo: TimeDilation = %.2f"), TimeDilation);
}

// ========== 디버그 유틸리티 ==========

void UExCoreCheatExtension::ExShowDebugAll()
{
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!GI) return;

	UExDebugStateSubsystem* DS = GI->GetSubsystem<UExDebugStateSubsystem>();
	if (!DS) return;

	bAllDebugEnabled = !bAllDebugEnabled;

	// 모든 Ex.Debug.* 태그를 일괄 토글
	DS->SetCheatEnabled(TAG_Ex_Debug_Path, bAllDebugEnabled);
	DS->SetCheatEnabled(TAG_Ex_Debug_Chunk, bAllDebugEnabled);
	DS->SetCheatEnabled(TAG_Ex_Debug_Slope, bAllDebugEnabled);
	DS->SetCheatEnabled(TAG_Ex_Debug_Speed, bAllDebugEnabled);
	DS->SetCheatEnabled(TAG_Ex_Debug_Collision, bAllDebugEnabled);

	UE_LOG(LogExCoreCheats, Log, TEXT("ExShowDebugAll: 전체 디버그 = %s"), 
		bAllDebugEnabled ? TEXT("ON") : TEXT("OFF"));
}

void UExCoreCheatExtension::ExSetDebugValue(FString Category, float Value)
{
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!GI) return;

	UExDebugStateSubsystem* DS = GI->GetSubsystem<UExDebugStateSubsystem>();
	if (!DS) return;

	const FString TagString = FString::Printf(TEXT("Ex.Debug.%s"), *Category);
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagString), false);

	if (!Tag.IsValid())
	{
		UE_LOG(LogExCoreCheats, Warning, TEXT("ExSetDebugValue: 유효하지 않은 카테고리 '%s'"), *Category);
		return;
	}

	DS->SetCheatValue(Tag, Value);
	DS->SetCheatEnabled(Tag, true); // 수치 설정 시 자동 활성화
	UE_LOG(LogExCoreCheats, Log, TEXT("ExSetDebugValue: [%s] = %.2f"), *Category, Value);
}
