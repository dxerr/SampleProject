// Copyright ExFrameWork. All Rights Reserved.

#include "ExObstacleManager.h"
#include "ExChunkSpawner.h" // For Delegate definition
#include "../Actors/ExFloorChunk.h"
#include "../Data/ExObstacleDefinition.h"
#include "../Data/ExObstacleSpawnStrategy.h"
#include "ExObstacleInteractionComponent.h"

#include "Kismet/GameplayStatics.h"
#include "Components/BoxComponent.h"
#include "../GameModes/ExRunnerGameMode.h"

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
		Spawner->OnWorldShifted.AddDynamic(this, &UExObstacleManager::OnWorldShifted);
		
		UE_LOG(LogExObstacleManager, Log, TEXT("Bound to ChunkSpawner: %s"), *Spawner->GetName());
	}
}

void UExObstacleManager::OnWorldShifted(float DeltaX)
{
	// 월드 시프트에 맞춰 좌표 보정
	// LastObstacleSafeEndX는 월드 좌표이므로, 모든 액터가 이동한 만큼 함께 이동해야 함.
	// Spawner::ShiftWorld에서 DeltaX만큼 액터를 이동시키므로(Current += Delta), 여기서도 더해줌.
	if (LastObstacleSafeEndX > -50000.f) // 초기값(-99999)이 아닐 때만
	{
		LastObstacleSafeEndX += DeltaX;
		UE_LOG(LogExObstacleManager, Verbose, TEXT("World Shifted by %.2f. New SafeEnd: %.2f"), DeltaX, LastObstacleSafeEndX);
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

	// ── 공통 로직: 랜덤 장애물 선택 ──
	UExObstacleDefinition* SelectedDef = SelectRandomDefinition();
	if (!SelectedDef || !SelectedDef->ObstacleClass) return;

	// ── 공통 로직: 타입별 전략 찾기 ──
	UExObstacleSpawnStrategy* Strategy = nullptr;
	if (TObjectPtr<UExObstacleSpawnStrategy>* Found = SpawnStrategies.Find(SelectedDef->Type))
	{
		Strategy = *Found;
	}

	if (!Strategy)
	{
		UE_LOG(LogExObstacleManager, Warning,
			TEXT("Type [%d]에 대한 SpawnStrategy가 설정되지 않았습니다. 장애물 생략."),
			(int32)SelectedDef->Type);
		return;
	}

	// ── 공통 로직: 배치 가능성 검사 ──
	float ChunkWorldStartX = Chunk->GetActorLocation().X - (ChunkLength * 0.5f);
	float SafeStartX = LastObstacleSafeEndX;
	if (SafeStartX < ChunkWorldStartX) SafeStartX = ChunkWorldStartX;

	float ObsLen = SelectedDef->MaxSize.X;
	float ChunkWorldEndX = Chunk->GetActorLocation().X + (ChunkLength * 0.5f);

	// ── Strategy 위임: 스폰 위치 계산 ──
	FVector SpawnPos = Strategy->CalculateSpawnPosition(SelectedDef, Chunk, SafeStartX);

	// 청크 범위 초과 체크
	if (SpawnPos.X + ObsLen > ChunkWorldEndX) return;

	// ── 공통 로직: 풀에서 가져오기 ──
	AActor* Obstacle = GetObstacleFromPool(SelectedDef->ObstacleClass);
	if (!Obstacle) return;

	// ── Strategy 위임: 장애물 설정 (스케일, 크기 등) ──
	Strategy->ConfigureObstacle(Obstacle, SelectedDef, Chunk);

	// ── 공통 로직: Interaction Component 설정 ──
	UExObstacleInteractionComponent* InteractionComp = Obstacle->FindComponentByClass<UExObstacleInteractionComponent>();
	if (InteractionComp)
	{
		FVector Origin, BaseExtent;
		Obstacle->GetActorBounds(true, Origin, BaseExtent);
		InteractionComp->SetBoxExtent(BaseExtent);
		InteractionComp->SetRelativeLocation(FVector::ZeroVector);
	}

	// ── 공통 로직: 위치 결정 및 어태치 ──
	FVector RelLoc = Chunk->GetActorTransform().InverseTransformPosition(SpawnPos);
	Obstacle->AttachToActor(Chunk, FAttachmentTransformRules::KeepWorldTransform);
	Obstacle->SetActorRelativeLocation(RelLoc);
	Obstacle->SetActorHiddenInGame(false);

	// ── Strategy 위임: 복귀 거리 계산 ──
	float RunSpeed = 600.f;
	if (AExRunnerGameMode* GM = Cast<AExRunnerGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		RunSpeed = GM->GetCurrentTreadmillSpeed();
	}

	float RecoveryDist = Strategy->GetRecoveryDistance(SelectedDef, RunSpeed);
	LastObstacleSafeEndX = SpawnPos.X + (ObsLen * 0.5f) + RecoveryDist;

	UE_LOG(LogExObstacleManager, Verbose,
		TEXT("Obstacle Spawned [Type:%d]: %s at (%.2f, %.2f, %.2f)"),
		(int32)SelectedDef->Type, *Obstacle->GetName(),
		SpawnPos.X, SpawnPos.Y, SpawnPos.Z);
}

UExObstacleDefinition* UExObstacleManager::SelectRandomDefinition() const
{
	if (ObstacleDefinitions.Num() == 0) return nullptr;

	const int32 Index = FMath::RandRange(0, ObstacleDefinitions.Num() - 1);
	return ObstacleDefinitions[Index];
}

void UExObstacleManager::ActivateObstacle(AActor* Obstacle)
{
	if (!IsValid(Obstacle)) return;

	// 풀에서 재활용 시 이전 상태 완전 초기화
	Obstacle->SetActorScale3D(FVector::OneVector);
	Obstacle->SetActorRotation(FRotator::ZeroRotator);

	Obstacle->SetActorHiddenInGame(false);
	Obstacle->SetActorEnableCollision(true);
	Obstacle->SetActorTickEnabled(true);

	// 모든 Primitive Component (Mesh, Collision 등)의 가시성 강제 초기화
	TArray<UPrimitiveComponent*> Comps;
	Obstacle->GetComponents<UPrimitiveComponent>(Comps);
	for (UPrimitiveComponent* Comp : Comps)
	{
		if (Comp)
		{
			Comp->SetHiddenInGame(false);
			Comp->SetVisibility(true, true);
		}
	}

	// 스케일/트랜스폼 변경을 컴포넌트에 즉시 반영
	Obstacle->UpdateComponentTransforms();
	Obstacle->MarkComponentsRenderStateDirty();
}

void UExObstacleManager::DeactivateObstacle(AActor* Obstacle)
{
	if (!IsValid(Obstacle)) return;

	Obstacle->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Obstacle->SetActorHiddenInGame(true);
	Obstacle->SetActorEnableCollision(false);
	Obstacle->SetActorTickEnabled(false);
}

AActor* UExObstacleManager::GetObstacleFromPool(UClass* ObstacleClass)
{
	if (!ObstacleClass) return nullptr;

	if (ObstaclePool.Contains(ObstacleClass))
	{
		TArray<AActor*>& Pool = ObstaclePool[ObstacleClass];
		
		// FIFO (First-In First-Out) 방식 적용
		// 오래된(먼저 반환된) 객체부터 사용하여 상태 안정화 시간 확보
		if (Pool.Num() > 0)
		{
			AActor* PooledActor = Pool[0];
			Pool.RemoveAt(0); // Pop() 대신 RemoveAt(0) 사용

			if (IsValid(PooledActor))
			{
				ActivateObstacle(PooledActor);
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

	DeactivateObstacle(Obstacle);

	UClass* Key = Obstacle->GetClass();
	
	if (!ObstaclePool.Contains(Key))
	{
		ObstaclePool.Add(Key, TArray<AActor*>());
	}
	ObstaclePool[Key].Add(Obstacle);
}
