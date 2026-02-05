// Copyright ExFrameWork. All Rights Reserved.

#include "ExCoreGameMode.h"
#include "../Data/ExCoreSpawnDataAsset.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"
#include "../Components/ExChunkSpawner.h"
#include "../Components/ExObstacleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogExCoreGameMode, Log, All);

AExCoreGameMode::AExCoreGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // bRunnerModeEnabled에 따라 활성화

	// 청크 스포너 컴포넌트 생성
	ChunkSpawner = CreateDefaultSubobject<UExChunkSpawner>(TEXT("ChunkSpawner"));
	ObstacleManager = CreateDefaultSubobject<UExObstacleManager>(TEXT("ObstacleManager"));
}

void AExCoreGameMode::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogExCoreGameMode, Log, TEXT("ExCoreGameMode BeginPlay - SpawnDataAsset: %s"), 
		SpawnDataAsset ? *SpawnDataAsset->GetName() : TEXT("None"));

	// 러너 모드가 활성화되어 있으면 자동으로 시작
	if (bRunnerModeEnabled)
	{
		StartRunnerGame();
		UE_LOG(LogExCoreGameMode, Log, TEXT("Runner Game Auto-Started! Speed: %.2f"), CurrentGameSpeed);
	}
}

void AExCoreGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	UE_LOG(LogExCoreGameMode, Log, TEXT("HandleStartingNewPlayer: %s"), *GetNameSafe(NewPlayer));
	
	// 부모 구현 호출 - SpawnDefaultPawnAtTransform 등이 호출됨
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

APawn* AExCoreGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogExCoreGameMode, Error, TEXT("SpawnDefaultPawnAtTransform: World is null"));
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
		UE_LOG(LogExCoreGameMode, Warning, TEXT("SpawnDefaultPawnAtTransform: No valid PawnClass to spawn"));
		return nullptr;
	}

	UE_LOG(LogExCoreGameMode, Log, TEXT("Spawning Pawn: %s at %s"), 
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
		UE_LOG(LogExCoreGameMode, Error, TEXT("Failed to spawn Pawn"));
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

	UE_LOG(LogExCoreGameMode, Log, TEXT("Successfully spawned: %s"), *SpawnedPawn->GetName());
	return SpawnedPawn;
}

AActor* AExCoreGameMode::ApplyVisualOverride(APawn* ContainerPawn, TSubclassOf<AActor> VisualClass)
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

	UE_LOG(LogExCoreGameMode, Log, TEXT("Applying VisualOverride: %s to %s"), 
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
		UE_LOG(LogExCoreGameMode, Error, TEXT("Failed to spawn VisualActor"));
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

			UE_LOG(LogExCoreGameMode, Log, TEXT("Animation copied from Visual: Mesh=%s, AnimClass=%s"),
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
		
		UE_LOG(LogExCoreGameMode, Warning, TEXT("ContainerPawn has no SkeletalMeshComponent, attached to RootComponent"));
	}

	// 추적 맵에 등록
	SpawnedVisualActors.Add(ContainerPawn, VisualActor);

	UE_LOG(LogExCoreGameMode, Log, TEXT("VisualOverride applied successfully: %s"), *VisualActor->GetName());
	return VisualActor;
}


void AExCoreGameMode::ChangeVisualOverride(APawn* TargetPawn, int32 NewVisualIndex)
{
	if (!TargetPawn || !SpawnDataAsset)
	{
		return;
	}

	// 인덱스 유효성 검사
	if (!SpawnDataAsset->VisualOverrides.IsValidIndex(NewVisualIndex))
	{
		UE_LOG(LogExCoreGameMode, Warning, TEXT("ChangeVisualOverride: Invalid index %d"), NewVisualIndex);
		return;
	}

	SpawnDataAsset->SelectedVisualIndex = NewVisualIndex;
	TSubclassOf<AActor> NewVisualClass = SpawnDataAsset->VisualOverrides[NewVisualIndex];
	
	ApplyVisualOverride(TargetPawn, NewVisualClass);
}

// ========== 러너 게임 시스템 구현 ==========

void AExCoreGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bRunnerModeEnabled)
	{
		return;
	}

	// 속도 가속
	CurrentGameSpeed += SpeedAcceleration * DeltaTime;

	// 이동 거리 누적 (Distance Matching용)
	TotalDistance += CurrentGameSpeed * DeltaTime;

	// [World Shift] 청크들을 뒤로 이동 (Treadmill)
	// 일시 정지 상태(등반 중)가 아닐 때만 이동
	if (ChunkSpawner && !bTreadmillPaused)
	{
		ChunkSpawner->ShiftWorld(-CurrentGameSpeed * DeltaTime);
	}

	UE_LOG(LogExCoreGameMode, Verbose, TEXT("Runner Tick - Speed: %.2f, Distance: %.2f"), 
		CurrentGameSpeed, TotalDistance);
}

void AExCoreGameMode::StartRunnerGame()
{
	bRunnerModeEnabled = true;
	CurrentGameSpeed = BaseGameSpeed;
	TotalDistance = 0.f;
	
	SetActorTickEnabled(true);

	// 스포너 초기화
	if (ChunkSpawner)
	{
		if (ObstacleManager)
		{
			ObstacleManager->BindToSpawner(ChunkSpawner);
		}

		ChunkSpawner->InitializeSpawner();
		UE_LOG(LogExCoreGameMode, Log, TEXT("ExChunkSpawner initialized."));
	}
	else
	{
		// 혹시나 블루프린트에서 제거했거나 실패했을 경우 동적 검색
		UExChunkSpawner* ComponentSpawner = GetComponentByClass<UExChunkSpawner>();
		if (ComponentSpawner)
		{
			ComponentSpawner->InitializeSpawner();
			UE_LOG(LogExCoreGameMode, Log, TEXT("ExChunkSpawner component found dynamically."));
		}
		else
		{
			UE_LOG(LogExCoreGameMode, Warning, TEXT("ExChunkSpawner component NOT found!"));
		}
	}

	UE_LOG(LogExCoreGameMode, Log, TEXT("Runner Game Started - BaseSpeed: %.2f"), BaseGameSpeed);
}

void AExCoreGameMode::StopRunnerGame()
{
	bRunnerModeEnabled = false;
	CurrentGameSpeed = 0.f;
	
	SetActorTickEnabled(false);

	SetActorTickEnabled(false);

	UE_LOG(LogExCoreGameMode, Log, TEXT("Runner Game Stopped - TotalDistance: %.2f"), TotalDistance);
}

void AExCoreGameMode::SetTreadmillPaused(bool bPaused)
{
	if (bTreadmillPaused != bPaused)
	{
		bTreadmillPaused = bPaused;
		UE_LOG(LogExCoreGameMode, Log, TEXT("Treadmill Paused: %s (Climbing/Interaction)"), bPaused ? TEXT("True") : TEXT("False"));
	}
}

