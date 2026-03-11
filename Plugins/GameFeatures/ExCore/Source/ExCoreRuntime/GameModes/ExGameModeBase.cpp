// Fill out your copyright notice in the Description page of Project Settings.

#include "GameModes/ExGameModeBase.h"
#include "GameModes/ExGameStateBase.h"
#include "Tags/ExMatchTags.h"
#include "Subsystems/ExGameFlowSubsystem.h"
#include "../Data/ExCoreSpawnDataAsset.h"
#include "Experience/ExExperienceManagerComponent.h"
#include "Experience/ExExperienceDefinition.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogExCoreGM, Log, All);

AExGameModeBase::AExGameModeBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // [Fix] 불필요한 빈 Tick 활성화 방지

	// Seamless Travel 사용 (주인님 요청사항)
	bUseSeamlessTravel = true;
	
	// 매치 시작 자동화 여부 기본값 
	bAutoStartOnReady = false;
}

void AExGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	// 전역 앱 플로우(GameFlowSubsystem)의 Travel Request 등록
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UExGameFlowSubsystem* FlowSubsystem = GI->GetSubsystem<UExGameFlowSubsystem>())
		{
			FlowSubsystem->OnRequestTravel.AddDynamic(this, &AExGameModeBase::OnFlowSubsystemRequestTravel);
		}
	}

	// GameState의 ExperienceManager에 할당된 DataAsset 주입
	if (DefaultExperience)
	{
		if (AGameStateBase* GS = GetGameState<AGameStateBase>())
		{
			if (UExExperienceManagerComponent* ExpManager = GS->GetComponentByClass<UExExperienceManagerComponent>())
			{
				ExpManager->ServerSetCurrentExperience(DefaultExperience);
				UE_LOG(LogExCoreGM, Log, TEXT("[ExGameModeBase] DefaultExperience (%s) automatically injected to Manager."), *DefaultExperience->GetName());
			}
		}
	}

	UE_LOG(LogExCoreGM, Log, TEXT("[ExGameModeBase] BeginPlay - SpawnDataAsset: %s"), 
		SpawnDataAsset ? *SpawnDataAsset->GetName() : TEXT("None"));
}

void AExGameModeBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UExGameFlowSubsystem* FlowSubsystem = GI->GetSubsystem<UExGameFlowSubsystem>())
		{
			FlowSubsystem->OnRequestTravel.RemoveDynamic(this, &AExGameModeBase::OnFlowSubsystemRequestTravel);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AExGameModeBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AExGameModeBase::SetMatchPhase(FGameplayTag NewPhase, bool bForceTransition)
{
	AExGameStateBase* ExGameState = GetGameState<AExGameStateBase>();
	if (!ExGameState)
	{
		return;
	}

	FGameplayTag CurrentPhase = ExGameState->GetCurrentMatchPhase();
	if (CurrentPhase == NewPhase)
	{
		return;
	}

	// [주인님 요청] 상태 전이 유효성 검사 제거 - 모든 상태 전이를 자유롭게 허용
	// (기존 AllowedMatchTransitions 맵 기반 검사 삭제)
	UE_LOG(LogExCoreGM, Log, TEXT("[ExGameModeBase] MatchPhase Transition Requested: %s -> %s"), *CurrentPhase.ToString(), *NewPhase.ToString());

	FGameplayTag OldPhase = CurrentPhase;
	ExGameState->CurrentMatchPhase = NewPhase; // friend 선언으로 접근 가능
	
	// 서버 자신도 로컬 델리게이트를 돌도록 강제 트리거
	ExGameState->OnRep_MatchPhase(OldPhase);

	// 매치 시작/종료 게임모드 콜백
	if (NewPhase == ExMatchTags::Match_Playing && OldPhase != ExMatchTags::Match_Playing)
	{
		OnMatchStarted();
	}
	else if (NewPhase == ExMatchTags::Match_PostMatch && OldPhase != ExMatchTags::Match_PostMatch)
	{
		OnMatchEnded();
	}
}

void AExGameModeBase::OnFlowSubsystemRequestTravel(const FString& MapURL)
{
	if (UWorld* World = GetWorld())
	{
		UE_LOG(LogExCoreGM, Log, TEXT("[ExGameModeBase] Performing ServerTravel to URL: %s with Seamless: %s"), *MapURL, bUseSeamlessTravel ? TEXT("True") : TEXT("False"));
		// bAbsolute=false (상대경로 유지), bShouldSkipGameNotify=false (보통 false)
		World->ServerTravel(MapURL, false, bUseSeamlessTravel);
	}
}

void AExGameModeBase::CheckAndStartMatch()
{
	AExGameStateBase* ExGameState = GetGameState<AExGameStateBase>();
	if (!ExGameState)
	{
		return;
	}

	if (ExGameState->GetCurrentMatchPhase() == ExMatchTags::Match_WaitingForPlayers)
	{
		if (CheckAllPlayersReady())
		{
			if (bAutoStartOnReady)
			{
				SetMatchPhase(ExMatchTags::Match_Playing);
				UE_LOG(LogExCoreGM, Log, TEXT("[ExGameModeBase] CheckAndStartMatch: All players ready and bAutoStartOnReady is true. Transitioned to Match_Playing."));
			}
			else
			{
				UE_LOG(LogExCoreGM, Log, TEXT("[ExGameModeBase] CheckAndStartMatch: All players ready but bAutoStartOnReady is false. Waiting for explicit start command."));
			}
		}
	}
}

bool AExGameModeBase::CheckAllPlayersReady() const
{
	if (!GetWorld()) return false;
	
	// 실제 구현에서는 AExPlayerControllerBase 등을 캐싱하여 Pawn의 Possess 완료 여부 등을 검증합니다.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC || !PC->HasClientLoadedCurrentWorld())
		{
			return false;
		}
	}
	return true;
}

void AExGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	// 접속 기록 등 커스텀 로직 수행 가능
}

void AExGameModeBase::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	
	// [Fix] 입장 시 무조건 Playing으로 전환하지 않고, CheckAndStartMatch 의도에 맡깁니다.
	CheckAndStartMatch();
}

AActor* AExGameModeBase::ChoosePlayerStart_Implementation(AController* Player)
{
	return Super::ChoosePlayerStart_Implementation(Player);
}

APawn* AExGameModeBase::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogExCoreGM, Error, TEXT("[ExGameModeBase] SpawnDefaultPawnAtTransform: World is null"));
		return nullptr;
	}

	// 1. 스폰할 Pawn 클래스 결정
	TSubclassOf<APawn> PawnClassToSpawn = nullptr;
	
	if (SpawnDataAsset)
	{
		PawnClassToSpawn = SpawnDataAsset->GetSelectedPawnClass();
	}
	
	// DataAsset에 설정이 없으면 DefaultPawnClass 사용
	if (!PawnClassToSpawn)
	{
		PawnClassToSpawn = DefaultPawnClass;
	}

	if (!PawnClassToSpawn)
	{
		UE_LOG(LogExCoreGM, Warning, TEXT("[ExGameModeBase] SpawnDefaultPawnAtTransform: No valid PawnClass to spawn"));
		return nullptr;
	}

	UE_LOG(LogExCoreGM, Log, TEXT("[ExGameModeBase] Spawning Pawn: %s at %s"), 
		*PawnClassToSpawn->GetName(), *SpawnTransform.ToString());

	// 2. 컨테이너 폰 스폰
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	SpawnParams.Owner = this;

	APawn* SpawnedPawn = World->SpawnActor<APawn>(
		PawnClassToSpawn, 
		SpawnTransform.GetLocation(), 
		SpawnTransform.Rotator(), 
		SpawnParams
	);

	if (!SpawnedPawn)
	{
		UE_LOG(LogExCoreGM, Error, TEXT("[ExGameModeBase] Failed to spawn Pawn"));
		return nullptr;
	}

	// 3. Visual Override 적용
	if (SpawnDataAsset)
	{
		TSubclassOf<AActor> VisualClass = SpawnDataAsset->GetSelectedVisualOverride();
		if (VisualClass)
		{
			ApplyVisualOverride(SpawnedPawn, VisualClass);
		}
	}

	UE_LOG(LogExCoreGM, Log, TEXT("[ExGameModeBase] Successfully spawned: %s"), *SpawnedPawn->GetName());
	return SpawnedPawn;
}

AActor* AExGameModeBase::ApplyVisualOverride(APawn* ContainerPawn, TSubclassOf<AActor> VisualClass)
{
	if (!ContainerPawn || !VisualClass)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UE_LOG(LogExCoreGM, Log, TEXT("[ExGameModeBase] Applying VisualOverride: %s to %s"), 
		*VisualClass->GetName(), *ContainerPawn->GetName());

	// 기존 Visual Actor가 있다면 제거
	if (AActor** ExistingVisual = SpawnedVisualActors.Find(ContainerPawn))
	{
		if (*ExistingVisual)
		{
			(*ExistingVisual)->Destroy();
		}
		SpawnedVisualActors.Remove(ContainerPawn);
	}

	// Visual Actor 스폰
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = ContainerPawn;

	AActor* VisualActor = World->SpawnActor<AActor>(
		VisualClass, 
		ContainerPawn->GetActorLocation(), 
		ContainerPawn->GetActorRotation(), 
		SpawnParams
	);

	if (!VisualActor)
	{
		UE_LOG(LogExCoreGM, Error, TEXT("[ExGameModeBase] Failed to spawn VisualActor"));
		return nullptr;
	}

	// 컨테이너 폰의 SkeletalMeshComponent 찾기 (ACharacter, APawn 모두 지원)
	USkeletalMeshComponent* ContainerMesh = nullptr;
	
	// 1. ACharacter인 경우 GetMesh() 사용
	ACharacter* ContainerCharacter = Cast<ACharacter>(ContainerPawn);
	if (ContainerCharacter)
	{
		ContainerMesh = ContainerCharacter->GetMesh();
	}
	
	// 2. 일반 APawn인 경우 동적으로 SkeletalMeshComponent 검색
	if (!ContainerMesh)
	{
		ContainerMesh = ContainerPawn->FindComponentByClass<USkeletalMeshComponent>();
	}

	// Visual Actor의 SkeletalMeshComponent 찾기 (Animation 설정 복사용)
	USkeletalMeshComponent* VisualMesh = VisualActor->FindComponentByClass<USkeletalMeshComponent>();

	// Visual Actor를 컨테이너 폰에 부착
	if (ContainerMesh)
	{
		// SkeletalMeshComponent가 있는 경우 해당 컴포넌트에 부착
		VisualActor->AttachToComponent(
			ContainerMesh, 
			FAttachmentTransformRules::SnapToTargetIncludingScale
		);

		// 컨테이너 메시 숨기기 (옵션)
		if (SpawnDataAsset && SpawnDataAsset->bHideContainerMesh)
		{
			ContainerMesh->SetHiddenInGame(true);
			ContainerMesh->SetVisibility(false);
		}

		// Visual Actor에서 Animation 설정을 ContainerMesh에 적용
		// 이렇게 하면 ContainerPawn이 VisualActor의 Animation으로 구동됨
		if (VisualMesh && SpawnDataAsset && SpawnDataAsset->bCopyAnimationFromVisual)
		{
			// Visual의 SkeletalMesh를 컨테이너에 적용
			USkeletalMesh* VisualSkeletalMesh = VisualMesh->GetSkeletalMeshAsset();
			if (VisualSkeletalMesh)
			{
				ContainerMesh->SetSkeletalMesh(VisualSkeletalMesh, false);
			}

			// Visual의 AnimClass를 컨테이너에 적용
			TSubclassOf<UAnimInstance> VisualAnimClass = VisualMesh->GetAnimClass();
			// Apply Animation
			if (VisualAnimClass)
			{
				ContainerMesh->SetAnimInstanceClass(VisualAnimClass);
			}

			UE_LOG(LogExCoreGM, Log, TEXT("[ExGameModeBase] Animation copied from Visual: Mesh=%s, AnimClass=%s"),
				VisualSkeletalMesh ? *VisualSkeletalMesh->GetName() : TEXT("None"),
				VisualAnimClass ? *VisualAnimClass->GetName() : TEXT("None"));

			// Visual Actor의 메시는 숨기기 (ContainerMesh가 대신 렌더링)
			VisualMesh->SetHiddenInGame(true);
			VisualMesh->SetVisibility(false);
			
			// 컨테이너 메시 다시 표시 (Visual의 외형을 보여줌)
			ContainerMesh->SetHiddenInGame(false);
			ContainerMesh->SetVisibility(true);
		}
	}
	else if (ContainerPawn->GetRootComponent())
	{
		// SkeletalMeshComponent가 없는 경우 RootComponent에 부착
		VisualActor->AttachToComponent(
			ContainerPawn->GetRootComponent(), 
			FAttachmentTransformRules::SnapToTargetIncludingScale
		);
		
		UE_LOG(LogExCoreGM, Warning, TEXT("[ExGameModeBase] ContainerPawn has no SkeletalMeshComponent, attached to RootComponent"));
	}

	// 추적 맵에 등록
	SpawnedVisualActors.Add(ContainerPawn, VisualActor);

	UE_LOG(LogExCoreGM, Log, TEXT("[ExGameModeBase] VisualOverride applied successfully: %s"), *VisualActor->GetName());
	return VisualActor;
}


void AExGameModeBase::ChangeVisualOverride(APawn* TargetPawn, int32 NewVisualIndex)
{
	if (!TargetPawn || !SpawnDataAsset)
	{
		return;
	}

	// 인덱스 유효성 검사
	if (!SpawnDataAsset->VisualOverrides.IsValidIndex(NewVisualIndex))
	{
		UE_LOG(LogExCoreGM, Warning, TEXT("[ExGameModeBase] ChangeVisualOverride: Invalid index %d"), NewVisualIndex);
		return;
	}

	SpawnDataAsset->SelectedVisualIndex = NewVisualIndex;
	TSubclassOf<AActor> NewVisualClass = SpawnDataAsset->VisualOverrides[NewVisualIndex];
	
	ApplyVisualOverride(TargetPawn, NewVisualClass);
}

void AExGameModeBase::OnMatchStarted_Implementation()
{
}

void AExGameModeBase::OnMatchEnded_Implementation()
{
}
