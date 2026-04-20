// Copyright ExFrameWork. All Rights Reserved.

#include "ExObstacleManager.h"
#include "ExChunkSpawner.h" // For Delegate definition
#include "../Actors/ExFloorChunk.h"
#include "../Data/ExRunnerConfig.h"
#include "../Data/ExObstacleDefinition.h"
#include "../Data/ExObstacleSpawnStrategy.h"

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
	LastObstacleSafeEndDistance = -99999.f;
}

void UExObstacleManager::BindToSpawner(UExChunkSpawner* Spawner)
{
	if (Spawner)
	{
		BoundSpawner = Spawner;
		// OnChunkSpawned는 이제 중앙 제어(ExChunkSpawner)에서 순차적으로 직접 호출하므로 바인딩 해제
		Spawner->OnChunkDespawned.AddDynamic(this, &UExObstacleManager::OnChunkDespawned);
		
		UE_LOG(LogExObstacleManager, Log, TEXT("Bound to ChunkSpawner: %s"), *Spawner->GetName());
	}
}



void UExObstacleManager::OnChunkSpawned(AExFloorChunk* Chunk)
{
	// [레거시] 기존 델리게이트 바인딩 기반 스폰 로직은 중앙 제어 방식으로 대체됨.
	// 이제 ExChunkSpawner::SpawnNextChunk 마지막에 명시적으로 SpawnObstaclesOnChunk를 호출함.
}

void UExObstacleManager::OnChunkDespawned(AExFloorChunk* Chunk)
{
	if (!Chunk) return;

	// Cleanup Logic
	TArray<AActor*> AttachedActors;
	Chunk->GetAttachedActors(AttachedActors);
	for (AActor* Attached : AttachedActors)
	{
		// 이 매니저가 관리하는 장애물인 경우에만 풀로 반환 (코인 등 다른 액터가 오염되는 것 방지)
		bool bIsObstacle = false;
		for (const UExObstacleDefinition* Def : ObstacleDefinitions)
		{
			if (Def && Attached->IsA(Def->ObstacleClass))
			{
				bIsObstacle = true;
				break;
			}
		}

		if (bIsObstacle)
		{
			ReturnObstacleToPool(Attached);
		}
	}
}

void UExObstacleManager::RequestBeatSpawn()
{
	if (!BoundSpawner) return;

	// 현재 활성화된 청크 중 스폰할 곳 찾기
	// 보통 LastObstacleSafeEndDistance를 덮는 청크를 찾거나, 가장 먼 청크에 스폰 예약.
	// ChunkSpawner가 자신의 Queue를 가지고 있다면 가장 최근에 스폰된 청크를 타겟으로 삼을 수 있습니다.
	AExFloorChunk* TargetChunk = BoundSpawner->GetLatestChunk();
	if (TargetChunk)
	{
		// 임시로 해당 청크의 시작 지점으로 스폰 명령 (확률 체크 스킵 -> 무조건 스폰)
		SpawnObstaclesOnChunk(TargetChunk, 0.f, TargetChunk->ChunkLength, true);
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

void UExObstacleManager::SpawnObstaclesOnChunk(AExFloorChunk* Chunk, float ChunkStartLocalX, float ChunkLength, bool bForceSpawn)
{
	if (ObstacleDefinitions.Num() == 0) return;
	if (!Chunk) return;
	if (!BoundSpawner || !BoundSpawner->RunnerConfig.IsValid()) return;

	// 최대 장애물 갯수 제한 확인 로직 (옵션)
	if (BoundSpawner->RunnerConfig->ObstacleSpawn.MaxActiveObstacles > 0)
	{
		int32 ActiveObstacleCount = 0;
		// 현재 월드에 활성화된(보이는) 풀링 액터 세기
		for (const auto& Pair : ObstaclePool)
		{
			for (AActor* Actor : Pair.Value)
			{
				if (Actor && !Actor->IsHidden())
				{
					ActiveObstacleCount++;
				}
			}
		}

		if (ActiveObstacleCount >= BoundSpawner->RunnerConfig->ObstacleSpawn.MaxActiveObstacles)
		{
			return; // 제한 도달 시 스폰 생략
		}
	}

	// 스포너에 설정된 기본 확률을 통한 장애물 배치 결정
	// (비트 기반 포스 스폰일 경우는 무조건 통과)
	if (!bForceSpawn && FMath::FRand() > BoundSpawner->RunnerConfig->ObstacleSpawn.SpawnProbability)
	{
		return;
	}

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
	// [변경] World X가 아닌 Path Distance 기반 계산
	// ExChunkSpawner에서 PathDistance를 Center 기준으로 설정하므로, StartDist는 HalfLength를 뻼
	float ChunkStartDist = Chunk->PathDistance - (ChunkLength * 0.5f);
	float SafeStartDist = LastObstacleSafeEndDistance;
	
	// 이전 장애물 끝 지점이 현재 청크 시작보다 전이면, 현재 청크 시작부터
	if (SafeStartDist < ChunkStartDist) SafeStartDist = ChunkStartDist;

	float ObsLen = SelectedDef->MaxSize.X;
	float ChunkEndDist = ChunkStartDist + ChunkLength;
	
	// 실제 스폰 예정 거리 (스포너의 안전 거리 설정값 적용)
	float ActualSpawnDist = SafeStartDist + BoundSpawner->RunnerConfig->ObstacleSpawn.MinSafeDistance;

	// 청크 범위 초과 체크 (미리 검사)
	if (ActualSpawnDist + ObsLen > ChunkEndDist) return;

	// ── Strategy 위임: 스폰 위치 계산 ──
	// SafeStartX 인자에 SafeStartDist(거리) 전달
	FTransform SpawnTrans = Strategy->CalculateSpawnPosition(SelectedDef, Chunk, SafeStartDist);
	FVector SpawnPos = SpawnTrans.GetLocation();

	// ── 공통 로직: 풀에서 가져오기 ──
	AActor* Obstacle = GetObstacleFromPool(SelectedDef->ObstacleClass);
	if (!Obstacle) return;

	// ── Strategy 위임: 장애물 설정 (스케일, 크기 등) ──
	Strategy->ConfigureObstacle(Obstacle, SelectedDef, Chunk);

	// [Fix] ConfigureObstacle에서 설정한 스케일 유지
	// CalculateSpawnPosition은 Scale=(1,1,1)을 반환하므로, 이를 그대로 SetActorTransform하면 스케일이 초기화됨.
	FVector ConfiguredScale = Obstacle->GetActorScale3D();
	SpawnTrans.SetScale3D(ConfiguredScale);

	// ── 공통 로직: 위치 결정 및 어태치 ──
	// World Transform을 그대로 적용 후 Attach (KeepWorld)
	Obstacle->SetActorTransform(SpawnTrans);
	Obstacle->AttachToActor(Chunk, FAttachmentTransformRules::KeepWorldTransform);
	
	// SetActorHiddenInGame(false)는 ActivateObstacle에서 이미 수행됨 (Pool 사용 시)
	// 하지만 새로 생성된 경우를 위해 안전장치
	Obstacle->SetActorHiddenInGame(false);

	// ── Strategy 위임: 복귀 거리 계산 ──
	float RunSpeed = BoundSpawner->RunnerConfig->ObstacleSpawn.DefaultRunSpeed;
	if (AExRunnerGameMode* GM = Cast<AExRunnerGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		if (APawn* PlayerPawn = GM->GetCachedPlayerPawn())
		{
			RunSpeed = PlayerPawn->GetVelocity().Size();
			if (RunSpeed < 10.f)
			{
				RunSpeed = BoundSpawner->RunnerConfig->ObstacleSpawn.DefaultRunSpeed; // 움직이지 않을 때는 기본 속도 가정
			}
		}
	}

	float RecoveryDist = Strategy->GetRecoveryDistance(SelectedDef, RunSpeed);
	
	// [변경] 거리 기반 누적
	LastObstacleSafeEndDistance = ActualSpawnDist + (ObsLen * 0.5f) + RecoveryDist;

	UE_LOG(LogExObstacleManager, Verbose,
		TEXT("Obstacle Spawned [Type:%d]: %s at Dist=%.1f (Loc: %.2f, %.2f)"),
		(int32)SelectedDef->Type, *Obstacle->GetName(),
		ActualSpawnDist, SpawnPos.X, SpawnPos.Y);
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

	// 스포너에 풀링 설정 여부 사용 (스포너나 옵션 데이터가 없으면 안전하게 풀링 안함)
	bool bShouldPool = (BoundSpawner && BoundSpawner->RunnerConfig.IsValid()) ? BoundSpawner->RunnerConfig->ObstacleSpawn.bUsePooling : false;

	// 오브젝트 풀링 미사용 시 무조건 바로 새로 스폰
	if (!bShouldPool)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GetOwner();
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return GetWorld()->SpawnActor<AActor>(ObstacleClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	}

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

	// 스포너 설정 사용
	bool bShouldPool = (BoundSpawner && BoundSpawner->RunnerConfig.IsValid()) ? BoundSpawner->RunnerConfig->ObstacleSpawn.bUsePooling : false;

	// 풀링 사용 안 하면 아예 액터 소멸
	if (!bShouldPool)
	{
		Obstacle->Destroy();
		return;
	}

	DeactivateObstacle(Obstacle);

	UClass* Key = Obstacle->GetClass();
	
	if (!ObstaclePool.Contains(Key))
	{
		ObstaclePool.Add(Key, TArray<AActor*>());
	}
	ObstaclePool[Key].Add(Obstacle);
}

bool UExObstacleManager::QueryObstacleAtDistance(float PathDist, float QueryRadius, FExObstacleContext& OutContext) const
{
	OutContext = FExObstacleContext();

	if (!BoundSpawner)
	{
		return false;
	}

	// 활성 청크에 Attach된 장애물들을 순회하여 거리 기반 검색
	const TArray<TObjectPtr<AExFloorChunk>>& ActiveChunks = BoundSpawner->GetActiveChunks();

	for (const TObjectPtr<AExFloorChunk>& Chunk : ActiveChunks)
	{
		if (!Chunk) continue;

		float ChunkHalfLen = Chunk->ChunkLength * 0.5f;
		float ChunkStartDist = Chunk->PathDistance - ChunkHalfLen;
		float ChunkEndDist = Chunk->PathDistance + ChunkHalfLen;

		// 이 청크가 질의 범위에 포함되는지 빠르게 검사
		if (PathDist + QueryRadius < ChunkStartDist || PathDist - QueryRadius > ChunkEndDist)
		{
			continue;
		}

		TArray<AActor*> AttachedActors;
		Chunk->GetAttachedActors(AttachedActors);

		for (AActor* Attached : AttachedActors)
		{
			if (!Attached || Attached->IsHidden())
			{
				continue;
			}

			// 장애물 액터의 월드 위치를 PathDistance로 근사
			float ObstacleX = Attached->GetActorLocation().X;
			float ChunkX = Chunk->GetActorLocation().X;
			// 로컬 오프셋을 PathDistance에 매핑
			float ObstacleLocalDist = ChunkStartDist + (ObstacleX - (ChunkX - ChunkHalfLen * Chunk->GetActorForwardVector().X));

			// 대략적 거리 체크 (장애물 바운드 고려)
			FBoxSphereBounds ObsBounds = GetVisualBounds(Attached);
			float ObstacleHalfExtentX = ObsBounds.BoxExtent.X;

			// [Fix] PathDist(논리적 누적 거리)와 ActorLocation.X(월드 절대 좌표)를 비교하는 오류 수정
			// 사전에 계산된 ObstacleLocalDist(PathDistance 스페이스로 변환된 장애물의 위치)와 비교해야 함.
			float DistToObstacle = FMath::Abs(PathDist - ObstacleLocalDist);
			if (DistToObstacle > QueryRadius + ObstacleHalfExtentX)
			{
				continue;
			}

			// 장애물 Definition 매칭 (ObstacleDefinitions에서 클래스로 역참조)
			EExObstacleType FoundType = EExObstacleType::None;
			bool bFoundClimbable = false;
			float ClimbableThreshold = 200.f;

			for (const UExObstacleDefinition* Def : ObstacleDefinitions)
			{
				if (Def && Attached->IsA(Def->ObstacleClass))
				{
					FoundType = Def->Type;
					// Slide 타입: ClimbHeight가 설정되어 있고, 장애물 높이가 임계값 이하면 올라갈 수 있음
					if (FoundType == EExObstacleType::Slide)
					{
						float ObstacleHeight = ObsBounds.BoxExtent.Z * 2.f;
						bFoundClimbable = (ObstacleHeight <= Def->ClimbHeight);
						ClimbableThreshold = Def->ClimbHeight;
					}
					break;
				}
			}

			OutContext.bHasObstacle = true;
			OutContext.ObstacleType = FoundType;
			OutContext.ObstacleBounds = FBox(
				ObsBounds.Origin - ObsBounds.BoxExtent,
				ObsBounds.Origin + ObsBounds.BoxExtent);
			OutContext.ObstacleTopZ = ObsBounds.Origin.Z + ObsBounds.BoxExtent.Z;
			OutContext.ObstacleBottomZ = ObsBounds.Origin.Z - ObsBounds.BoxExtent.Z;
			OutContext.bCanClimbOver = bFoundClimbable;
			OutContext.ClimbableHeightThreshold = ClimbableThreshold;

			return true; // 가장 먼저 발견된 장애물 반환
		}
	}

	return false;
}

