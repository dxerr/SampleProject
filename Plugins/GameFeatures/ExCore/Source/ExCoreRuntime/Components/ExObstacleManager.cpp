// Copyright ExFrameWork. All Rights Reserved.

#include "ExObstacleManager.h"
#include "ExChunkSpawner.h" // For Delegate definition
#include "../Actors/ExFloorChunk.h"
#include "../Data/ExObstacleDefinition.h"
#include "ExObstacleInteractionComponent.h"

#include "Kismet/GameplayStatics.h"
#include "Components/BoxComponent.h"
#include "../GameModes/ExCoreGameMode.h"

DEFINE_LOG_CATEGORY_STATIC(LogExObstacleManager, Log, All);

UExObstacleManager::UExObstacleManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UExObstacleManager::BeginPlay()
{
	Super::BeginPlay();
	LastObstacleSafeEndX = -99999.f;
}

void UExObstacleManager::BindToSpawner(UExChunkSpawner* Spawner)
{
	if (Spawner)
	{
		Spawner->OnChunkSpawned.AddDynamic(this, &UExObstacleManager::OnChunkSpawned);
		Spawner->OnChunkDespawned.AddDynamic(this, &UExObstacleManager::OnChunkDespawned);
		
		UE_LOG(LogExObstacleManager, Log, TEXT("Bound to ChunkSpawner: %s"), *Spawner->GetName());
	}
}

void UExObstacleManager::OnChunkSpawned(AExFloorChunk* Chunk)
{
	if (!Chunk) return;

	// 바닥 청크의 길이를 가정 (기본 1000)하거나, Chunk에서 가져옴.
	// 여기서는 ChunkLength가 ExFloorChunk에 있다고 가정.
	float Length = Chunk->ChunkLength; // public member accessed
	
	SpawnObstaclesOnChunk(Chunk, 0.f, Length);
}

void UExObstacleManager::OnChunkDespawned(AExFloorChunk* Chunk)
{
	if (!Chunk) return;

	// Cleanup Logic
	TArray<AActor*> AttachedActors;
	Chunk->GetAttachedActors(AttachedActors);
	for (AActor* Attached : AttachedActors)
	{
		ReturnObstacleToPool(Attached);
	}
}

// --- Migrated Logic ---

FBoxSphereBounds UExObstacleManager::GetVisualBounds(AActor* Actor)
{
	if (!IsValid(Actor)) return FBoxSphereBounds(FVector::ZeroVector, FVector::ZeroVector, 0.f);

	// 1. Try to find a StaticMeshComponent
	TArray<UStaticMeshComponent*> MeshComps;
	Actor->GetComponents<UStaticMeshComponent>(MeshComps);

	for (UStaticMeshComponent* Mesh : MeshComps)
	{
		// Ignore hidden or collision-only meshes if complex
		if (Mesh && Mesh->GetStaticMesh())
		{
			return Mesh->Bounds; // World Space Bounds
		}
	}

	// 2. Fallback: Colliding Components
	FVector Origin, Extent;
	Actor->GetActorBounds(true, Origin, Extent);
	if (!Extent.IsZero())
	{
		return FBoxSphereBounds(Origin, Extent, Extent.GetMax());
	}

	// 3. Last Result: All Components
	Actor->GetActorBounds(false, Origin, Extent);
	return FBoxSphereBounds(Origin, Extent, Extent.GetMax());
}

void UExObstacleManager::SpawnObstaclesOnChunk(AExFloorChunk* Chunk, float ChunkStartLocalX, float ChunkLength)
{
	if (ObstacleDefinitions.Num() == 0) return;
	if (!Chunk) return;

	// 간단한 확률 체크 (임시: 30% 확률로 생성 안함)
	if (FMath::RandRange(0, 10) < 3) return;

	// 랜덤 장애물 선택
	int32 Index = FMath::RandRange(0, ObstacleDefinitions.Num() - 1);
	UExObstacleDefinition* SelectedDef = ObstacleDefinitions[Index];
	if (!SelectedDef || !SelectedDef->ObstacleClass) return;

	// 배치 가능성 검사
	float ChunkWorldStartX = Chunk->GetActorLocation().X - (ChunkLength * 0.5f);
	float SafeStartX = LastObstacleSafeEndX;
	if (SafeStartX < ChunkWorldStartX) SafeStartX = ChunkWorldStartX;

	float ObsLen = SelectedDef->MaxSize.X; 
	float SpawnWorldX = SafeStartX + 200.f; // Buffer

	float ChunkWorldEndX = Chunk->GetActorLocation().X + (ChunkLength * 0.5f);
	if (SpawnWorldX + ObsLen > ChunkWorldEndX) return;

	// --- 배치 실행 ---
	AActor* Obstacle = GetObstacleFromPool(SelectedDef->ObstacleClass);
	if (Obstacle)
	{
		// 1. 랜덤 크기 생성
		float TargetLength = FMath::RandRange(SelectedDef->MinSize.X, SelectedDef->MaxSize.X);
		float TargetHeight = FMath::RandRange(SelectedDef->MinSize.Z, SelectedDef->MaxSize.Z);
		float TargetWidth = 1000.f; 

		// 바닥 너비 구하기
		FBoxSphereBounds FloorBounds = GetVisualBounds(Chunk);
		float FloorHalfWidth = FloorBounds.BoxExtent.Y;
		if (FloorHalfWidth < 10.f) FloorHalfWidth = 500.f;
		TargetWidth = FloorHalfWidth * 2.0f;

		// 2. 스케일 적용
		Obstacle->SetActorScale3D(FVector::OneVector);
		FBoxSphereBounds ObsBounds = GetVisualBounds(Obstacle);
		FVector BaseSize = ObsBounds.BoxExtent * 2.0f;
		
		if (BaseSize.X < 1.f) BaseSize.X = 100.f;
		if (BaseSize.Y < 1.f) BaseSize.Y = 100.f;
		if (BaseSize.Z < 1.f) BaseSize.Z = 100.f;

		FVector NewScale;
		NewScale.X = TargetLength / BaseSize.X;
		NewScale.Y = TargetWidth / BaseSize.Y;
		NewScale.Z = TargetHeight / BaseSize.Z;
		
		Obstacle->SetActorScale3D(NewScale);

		// 3. Interaction Component 설정
		UExObstacleInteractionComponent* InteractionComp = Obstacle->FindComponentByClass<UExObstacleInteractionComponent>();
		if (InteractionComp)
		{
			InteractionComp->SetBoxExtent(BaseSize * 0.5f); 
			InteractionComp->SetRelativeLocation(FVector::ZeroVector);
		}

		// 4. 위치 결정
		FVector ChunkLoc = Chunk->GetActorLocation();
		// Pivot Adjustment: Subtract half-width to center Bottom-Left pivot object
		FVector TargetWorldPos(
			SpawnWorldX, 
			ChunkLoc.Y - (TargetWidth * 0.5f), 
			ChunkLoc.Z
		);

		FVector RelLoc = Chunk->GetActorTransform().InverseTransformPosition(TargetWorldPos);

		Obstacle->AttachToActor(Chunk, FAttachmentTransformRules::KeepWorldTransform);
		Obstacle->SetActorRelativeLocation(RelLoc);
		Obstacle->SetActorHiddenInGame(false);

		// Next Safe X 갱신
		float RunSpeed = 600.f;
		if (AExCoreGameMode* GM = Cast<AExCoreGameMode>(UGameplayStatics::GetGameMode(this)))
		{
			RunSpeed = GM->GetCurrentGameSpeed();
		}
		float RecoveryDist = RunSpeed * SelectedDef->RecoveryTime;
		LastObstacleSafeEndX = SpawnWorldX + (ObsLen * 0.5f) + RecoveryDist; 
		
		UE_LOG(LogExObstacleManager, Verbose, TEXT("Obstacle Spawned: %s at %.2f"), *Obstacle->GetName(), SpawnWorldX);
	}
}

AActor* UExObstacleManager::GetObstacleFromPool(UClass* ObstacleClass)
{
	if (!ObstacleClass) return nullptr;

	if (ObstaclePool.Contains(ObstacleClass))
	{
		TArray<AActor*>& Pool = ObstaclePool[ObstacleClass];
		if (Pool.Num() > 0)
		{
			AActor* PooledActor = Pool.Pop();
			if (IsValid(PooledActor))
			{
				PooledActor->SetActorHiddenInGame(false);
				PooledActor->SetActorEnableCollision(true);
				PooledActor->SetActorTickEnabled(true);
				return PooledActor;
			}
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* NewActor = GetWorld()->SpawnActor<AActor>(ObstacleClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	return NewActor;
}

void UExObstacleManager::ReturnObstacleToPool(AActor* Obstacle)
{
	if (!IsValid(Obstacle)) return;

	Obstacle->SetActorHiddenInGame(true);
	Obstacle->SetActorEnableCollision(false);
	Obstacle->SetActorTickEnabled(false);
	Obstacle->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	UClass* Key = Obstacle->GetClass();
	
	if (!ObstaclePool.Contains(Key))
	{
		ObstaclePool.Add(Key, TArray<AActor*>());
	}
	ObstaclePool[Key].Add(Obstacle);
}
