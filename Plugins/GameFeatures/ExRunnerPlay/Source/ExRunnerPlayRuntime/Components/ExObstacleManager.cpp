// Copyright ExFrameWork. All Rights Reserved.

#include "ExObstacleManager.h"
#include "ExChunkSpawner.h" // For Delegate definition
#include "../Actors/ExFloorChunk.h"
#include "../Data/ExRunnerConfig.h"
#include "../Data/ExObstacleDefinition.h"
#include "../Data/ExObstacleSpawnStrategy.h"
#include "../Interfaces/ExObstacleInterface.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Kismet/GameplayStatics.h"
#include "Components/BoxComponent.h"
#include "../GameModes/ExRunnerGameMode.h"
#include "Subsystems/ExDataCenterSubsystem.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogExObstacleManager, Log, All);

UExObstacleManager::UExObstacleManager()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(true);
}

void UExObstacleManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 클라이언트 전용: OnRep_ReplicatedObstacles Race Condition 폴링 해결
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		// 리플리케이트된 배열의 크기가 줄어들면, 게임 재시작/초기화 상황일 수 있으므로 Set 정리
		if (ClientAttachedObstacles.Num() > ReplicatedObstacles.Num())
		{
			ClientAttachedObstacles.Empty();
		}

		for (const FExObstacleSyncInfo& Info : ReplicatedObstacles)
		{
			// 장애물이 도착했고, 아직 클라이언트 측에서 동기화 처리를 완료하지 않았다면
			if (!Info.Obstacle)
			{
				UE_LOG(LogExObstacleManager, Log, TEXT("[Client Sync] Info.Obstacle이 아직 유효하지 않음. (SegmentIndex: %d)"), Info.SegmentIndex);
				continue;
			}

			if (ClientAttachedObstacles.Contains(Info.Obstacle))
			{
				continue;
			}

			if (BoundSpawner)
			{
				AExFloorChunk* TargetChunk = nullptr;
				for (AExFloorChunk* Chunk : BoundSpawner->GetActiveChunks())
				{
					if (Chunk && Chunk->SegmentIndex == Info.SegmentIndex)
					{
						TargetChunk = Chunk;
						break;
					}
				}

				if (TargetChunk)
				{
					UE_LOG(LogExObstacleManager, Log, TEXT("[Client Sync] 장애물 %s 수동 부착 및 동기화 적용. (SegmentIndex: %d)"), *Info.Obstacle->GetName(), Info.SegmentIndex);

					Info.Obstacle->AttachToActor(TargetChunk, FAttachmentTransformRules::KeepWorldTransform);
					ActivateObstacle(Info.Obstacle);

					// 서버가 CalculateSpawnPosition에서 계산한 최종 World Transform을 강제 적용.
					// AttachToActor(KeepWorldTransform)만으로는 ReplicatedMovement 기반 위치가 사용되며,
					// 커브 청크 이후 방향이 바뀐 구간에서 회전이 틀어지는 현상이 발생한다.
					// 서버 권위 데이터를 명시적으로 덮어써 클라이언트 시각 정합성을 보장한다.
					if (!Info.WorldLocation.IsZero() || !Info.WorldRotation.IsZero())
					{
						Info.Obstacle->SetActorLocationAndRotation(
							Info.WorldLocation,
							Info.WorldRotation,
							false,
							nullptr,
							ETeleportType::TeleportPhysics
						);
						UE_LOG(LogExObstacleManager, Log, TEXT("[Client Sync] 커브 구간 Transform 강제 적용: %s Loc=(%.1f,%.1f,%.1f) Rot=(P%.1f,Y%.1f,R%.1f)"),
							*Info.Obstacle->GetName(),
							Info.WorldLocation.X, Info.WorldLocation.Y, Info.WorldLocation.Z,
							Info.WorldRotation.Pitch, Info.WorldRotation.Yaw, Info.WorldRotation.Roll);
					}

					// 서버가 보내준 동기화 데이터 강제 적용
					Info.Obstacle->SetActorScale3D(Info.ActorScale);

					// 메쉬 스케일 보정 적용
					if (Info.MeshRelativeScaleY > 0.f)
					{
						TArray<UStaticMeshComponent*> MeshComps;
						Info.Obstacle->GetComponents<UStaticMeshComponent>(MeshComps);
						for (UStaticMeshComponent* Mesh : MeshComps)
						{
							if (Mesh)
							{
								FVector Scale = Mesh->GetRelativeScale3D();
								Mesh->SetRelativeScale3D(FVector(Scale.X, Info.MeshRelativeScaleY, Scale.Z));
							}
						}
					}

					// Gap 적용
					if (Info.GapLocalStartDist >= 0.f)
					{
						UE_LOG(LogExObstacleManager, Log, TEXT("[Client Sync] Gap 구멍 생성 요청. GapStart=%.1f, GapWidth=%.1f"), Info.GapLocalStartDist, Info.InfoValue);
						TargetChunk->ApplyGap(Info.GapLocalStartDist, Info.InfoValue);
					}

					// Interface를 통한 UI 정보 적용
					if (Info.Obstacle->GetClass()->ImplementsInterface(UExObstacleInterface::StaticClass()))
					{
						FExObstacleInfo ObsInfo;
						ObsInfo.Type = static_cast<EExObstacleType>(Info.InfoType);
						ObsInfo.Value = Info.InfoValue;

						// 단위 변환: cm -> m, 소수점 2자리 반올림 (ApplyObstacleInfo와 동일하게 처리)
						float MeterValue = ObsInfo.Value * 0.01f;
						ObsInfo.Value = FMath::RoundToFloat(MeterValue * 100.0f) / 100.0f;
						
						UE_LOG(LogExObstacleManager, Log, TEXT("[Client Sync] 장애물 %s 에 수치 정보 적용: Type=%d, Value=%.2f"), *Info.Obstacle->GetName(), Info.InfoType, ObsInfo.Value);
						IExObstacleInterface::Execute_SetupObstacleInfo(Info.Obstacle, ObsInfo);
					}
					
					TArray<TWeakObjectPtr<AActor>>& SegArray = SpawnedBySegment.FindOrAdd(Info.SegmentIndex);
					SegArray.AddUnique(Info.Obstacle);

					// 처리 완료 기록
					ClientAttachedObstacles.Add(Info.Obstacle);
				}
				else
				{
					UE_LOG(LogExObstacleManager, Log, TEXT("[Client Sync] 장애물 %s (SegIdx: %d)의 TargetChunk를 아직 찾지 못함. 대기 중..."), *Info.Obstacle->GetName(), Info.SegmentIndex);
				}
			}
		}
	}
}

void UExObstacleManager::BeginPlay()
{
	Super::BeginPlay();
	LastObstacleSafeEndDistance = -99999.f;

	// DataCenter에서 활성화된 플러그인의 모든 Obstacle Definition을 가져와 캐싱
	if (UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		if (UExDataCenterSubsystem* DataCenter = GI->GetSubsystem<UExDataCenterSubsystem>())
		{
			CachedObstacleDefinitions = DataCenter->GetAllDefinitions<UExObstacleDefinition>();
			UE_LOG(LogExObstacleManager, Log, TEXT("[ExObstacleManager] Loaded %d Obstacle Definitions from DataCenter"), CachedObstacleDefinitions.Num());
		}
	}
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
	// 이제 ExChunkSpawner::SpawnNextChunk 마지막에 명시적으로 GenerateObstaclePlan + RealizeObstaclePlan을 호출함.
}

void UExObstacleManager::OnChunkDespawned(AExFloorChunk* Chunk)
{
	if (!Chunk) return;

	// Phase 4: SpawnedBySegment 인덱스 기반 회수
	if (const TArray<TWeakObjectPtr<AActor>>* SegActors = SpawnedBySegment.Find(Chunk->SegmentIndex))
	{
		for (const TWeakObjectPtr<AActor>& WeakObstacle : *SegActors)
		{
			if (AActor* Obstacle = WeakObstacle.Get())
			{
				ReturnObstacleToPool(Obstacle);
			}
		}
		SpawnedBySegment.Remove(Chunk->SegmentIndex);
	}
	else
	{
		// 레거시 폴백: SpawnedBySegment에 없으면 Attach 기반 회수 시도 (이전 청크 대응)
		TArray<AActor*> AttachedActors;
		Chunk->GetAttachedActors(AttachedActors);
		for (AActor* Attached : AttachedActors)
		{
			bool bIsObstacle = false;
			for (const UExObstacleDefinition* Def : CachedObstacleDefinitions)
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

	// ── Phase 4: 리플리케이트 데이터 정리 (서버 전용) ──
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		int32 RemovedCount = ReplicatedObstacles.RemoveAll([TargetIndex = Chunk->SegmentIndex](const FExObstacleSyncInfo& Info) {
			return Info.SegmentIndex == TargetIndex || !IsValid(Info.Obstacle);
		});
		
		if (RemovedCount > 0)
		{
			MARK_PROPERTY_DIRTY_FROM_NAME(UExObstacleManager, ReplicatedObstacles, this);
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

// ── Phase 1: 결정론 RandomStream 초기화 ──

void UExObstacleManager::InitializeRandomStream(int32 SharedSeed)
{
	// Hash(SharedSeed, 1)로 Obstacle 전용 스트림 파생
	const int32 ObstacleSeed = HashCombine(SharedSeed, 1);
	ObstacleRandomStream.Initialize(ObstacleSeed);
	bRandomStreamInitialized = true;
	LastObstacleSafeEndDistance = -99999.f; // 시드 변경 시 반드시 리셋

	UE_LOG(LogExObstacleManager, Log, TEXT("[ExObstacleManager] RandomStream 초기화 완료 (SharedSeed=%d, ObstacleSeed=%d)"),
		SharedSeed, ObstacleSeed);
}

// ── Phase 2: Plan 기반 2단계 스폰 ──

TArray<FExSpawnPlan> UExObstacleManager::GenerateObstaclePlan(AExFloorChunk* Chunk)
{
	// 초기화 계약 위반 감지
	ensureMsgf(bRandomStreamInitialized,
		TEXT("[ExObstacleManager] GenerateObstaclePlan 호출 시점에 RandomStream이 초기화되지 않았습니다! InitializeRandomStream을 먼저 호출해야 합니다."));

	TArray<FExSpawnPlan> ResultPlan;

	if (!Chunk) return ResultPlan;
	if (CachedObstacleDefinitions.Num() == 0) return ResultPlan;
	if (!BoundSpawner || !BoundSpawner->RunnerConfig.IsValid()) return ResultPlan;

	// 최대 장애물 갯수 제한 확인
	if (BoundSpawner->RunnerConfig->ObstacleSpawn.MaxActiveObstacles > 0)
	{
		int32 ActiveObstacleCount = 0;
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
			return ResultPlan;
		}
	}

	// 스포너 설정 기반 스폰 확률 판정 (결정론 스트림 사용)
	if (ObstacleRandomStream.FRand() > BoundSpawner->RunnerConfig->ObstacleSpawn.SpawnProbability)
	{
		return ResultPlan;
	}

	// 랜덤 장애물 선택 (결정론 스트림 사용)
	if (CachedObstacleDefinitions.Num() == 0) return ResultPlan;
	const int32 DefIndex = ObstacleRandomStream.RandRange(0, CachedObstacleDefinitions.Num() - 1);
	UExObstacleDefinition* SelectedDef = CachedObstacleDefinitions[DefIndex];
	if (!SelectedDef || !SelectedDef->ObstacleClass) return ResultPlan;

	// 타입별 전략 찾기
	UExObstacleSpawnStrategy* Strategy = nullptr;
	if (TObjectPtr<UExObstacleSpawnStrategy>* Found = SpawnStrategies.Find(SelectedDef->Type))
	{
		Strategy = *Found;
	}
	if (!Strategy)
	{
		UE_LOG(LogExObstacleManager, Warning,
			TEXT("Type [%d]에 대한 SpawnStrategy가 설정되지 않았습니다. 장애물 Plan 생략."),
			(int32)SelectedDef->Type);
		return ResultPlan;
	}

	// 배치 가능성 검사
	const float ChunkHalfLen = Chunk->ChunkLength * 0.5f;
	const float ChunkStartDist = Chunk->PathDistance - ChunkHalfLen;
	const float ChunkEndDist = Chunk->PathDistance + ChunkHalfLen;

	float TargetSpawnDist = LastObstacleSafeEndDistance + BoundSpawner->RunnerConfig->ObstacleSpawn.MinSafeDistance;
	if (TargetSpawnDist < ChunkStartDist)
	{
		TargetSpawnDist = ChunkStartDist;
	}

	const float ObsLen = SelectedDef->MaxSize.X;
	if (TargetSpawnDist + ObsLen > ChunkEndDist)
	{
		return ResultPlan;
	}

	// Strategy 위임: 스폰 위치 계산
	const float ActualSpawnDist = TargetSpawnDist;
	FTransform SpawnTrans = Strategy->CalculateSpawnPosition(SelectedDef, Chunk, ActualSpawnDist);

	// Plan 생성 (정렬 불변식: PathDistance 오름차순으로 산출됨)
	FExSpawnPlan Plan;
	Plan.OwnerSegmentIndex = Chunk->SegmentIndex;
	Plan.LocalPathOffset = ActualSpawnDist - ChunkStartDist;
	Plan.WorldLocation = SpawnTrans.GetLocation();
	Plan.WorldRotation = SpawnTrans.Rotator();
	Plan.ActorClass = SelectedDef->ObstacleClass;
	Plan.ObstacleTypeRaw = (uint8)SelectedDef->Type;
	Plan.ScaleHint = FVector::OneVector; // ConfigureObstacle에서 설정됨

	// 안전 거리 갱신 (Generate 단계에서 상태값 갱신)
	float RunSpeed = BoundSpawner->RunnerConfig->ObstacleSpawn.DefaultRunSpeed;
	const float RecoveryDist = Strategy->GetRecoveryDistance(SelectedDef, RunSpeed);
	LastObstacleSafeEndDistance = ActualSpawnDist + (ObsLen * 0.5f) + RecoveryDist;

	ResultPlan.Add(Plan);

	UE_LOG(LogExObstacleManager, Verbose,
		TEXT("[GenerateObstaclePlan] Type=%d, Dist=%.1f, SegIdx=%d"),
		(int32)SelectedDef->Type, ActualSpawnDist, Chunk->SegmentIndex);

	return ResultPlan;
}

void UExObstacleManager::RealizeObstaclePlan(const TArray<FExSpawnPlan>& Plan, AExFloorChunk* Chunk)
{
	// §3.4 서버 권한 이중 가드: 차단 + 알림
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		ensureMsgf(false, TEXT("[ExObstacleManager] RealizeObstaclePlan이 클라이언트에서 호출됨. 서버 전용 함수입니다."));
		return;
	}

	if (!Chunk) return;

	for (const FExSpawnPlan& SpawnPlan : Plan)
	{
		if (!SpawnPlan.ActorClass) continue;

		// 풀에서 액터 가져오기
		AActor* Obstacle = GetObstacleFromPool(SpawnPlan.ActorClass);
		if (!Obstacle) continue;

		// Strategy 위임: 장애물 설정 (스케일, 크기 등)
		UExObstacleDefinition* MatchedDef = nullptr;
		for (UExObstacleDefinition* Def : CachedObstacleDefinitions)
		{
			if (Def && Obstacle->IsA(Def->ObstacleClass))
			{
				MatchedDef = Def;
				break;
			}
		}

		if (MatchedDef)
		{
			if (UExObstacleSpawnStrategy* Strategy = SpawnStrategies.FindRef(MatchedDef->Type))
			{
				Strategy->ConfigureObstacle(Obstacle, MatchedDef, Chunk);
			}
		}

		// Phase 4: World Space 스폰 (Attach 호출하지 않음)
		// Manual Re-Attach 모드: OwnerSegmentIndex를 Replicated 변수로 설정하여 클라이언트가 OnRep에서 수동 Attach
		FTransform SpawnTrans(SpawnPlan.WorldRotation, SpawnPlan.WorldLocation, Obstacle->GetActorScale3D());
		Obstacle->SetActorTransform(SpawnTrans);
		Obstacle->SetActorHiddenInGame(false);

		// SpawnedBySegment 인덱스에 등록 (양측 동일하게 유지)
		TArray<TWeakObjectPtr<AActor>>& SegArray = SpawnedBySegment.FindOrAdd(SpawnPlan.OwnerSegmentIndex);
		SegArray.Add(Obstacle);

		// ── Phase 4: 리플리케이션 상태 업데이트 ──
		FExObstacleSyncInfo SyncInfo;
		SyncInfo.Obstacle = Obstacle;
		SyncInfo.SegmentIndex = SpawnPlan.OwnerSegmentIndex;
		SyncInfo.ActorScale = Obstacle->GetActorScale3D();
		// 서버가 계산한 최종 월드 Transform을 저장하여 클라이언트에서 강제 적용할 수 있도록 함.
		// 커브 청크 이후 방향이 바뀌는 구간에서 ReplicatedMovement 만으로는 정확한 회전이 보장되지 않으므로,
		// 서버 권위 데이터를 직접 전달하여 클라이언트가 명시적으로 덮어쓴다.
		SyncInfo.WorldLocation = SpawnPlan.WorldLocation;
		SyncInfo.WorldRotation = SpawnPlan.WorldRotation;

		if (MatchedDef)
		{
			SyncInfo.InfoType = (uint8)MatchedDef->Type;
			if (UExObstacleSpawnStrategy* Strategy = SpawnStrategies.FindRef(MatchedDef->Type))
			{
				SyncInfo.InfoValue = Strategy->LastGeneratedInfoValue;
				SyncInfo.GapLocalStartDist = Strategy->LastGeneratedGapLocalStartDist;
				SyncInfo.MeshRelativeScaleY = Strategy->LastGeneratedMeshRelativeScaleY;
			}
		}

		ReplicatedObstacles.Add(SyncInfo);
		MARK_PROPERTY_DIRTY_FROM_NAME(UExObstacleManager, ReplicatedObstacles, this);

		UE_LOG(LogExObstacleManager, Verbose,
			TEXT("[RealizeObstaclePlan] Spawned %s at (%.1f, %.1f, %.1f), SegIdx=%d"),
			*Obstacle->GetName(),
			SpawnPlan.WorldLocation.X, SpawnPlan.WorldLocation.Y, SpawnPlan.WorldLocation.Z,
			SpawnPlan.OwnerSegmentIndex);
	}
}

bool UExObstacleManager::QueryObstaclePlanAtDistance(const TArray<FExSpawnPlan>& Plan, float PathDist, float QueryRadius, FExObstacleContext& OutContext) const
{
	OutContext = FExObstacleContext();

	for (const FExSpawnPlan& SpawnPlan : Plan)
	{
		// Plan의 글로벌 PathDistance는 ChunkStartDist + LocalPathOffset으로 계산됨
		// 그러나 Plan.WorldLocation.X를 PathDistance 근사로 활용
		// (트레드밀 구조에서 X좌표 ≈ PathDistance)
		const float PlanPathDist = SpawnPlan.WorldLocation.X;

		if (FMath::Abs(PathDist - PlanPathDist) > QueryRadius)
		{
			continue;
		}

		// Definition에서 장애물 정보 역참조
		const EExObstacleType ObstacleType = (EExObstacleType)SpawnPlan.ObstacleTypeRaw;

		// 바운드 근사 (Plan에는 정확한 Bounds가 없으므로 WorldLocation 기반 추정)
		OutContext.bHasObstacle = true;
		OutContext.ObstacleType = ObstacleType;
		OutContext.ObstacleBounds = FBox(
			SpawnPlan.WorldLocation - FVector(QueryRadius),
			SpawnPlan.WorldLocation + FVector(QueryRadius));
		OutContext.ObstacleTopZ = SpawnPlan.WorldLocation.Z + SpawnPlan.PlacedZ;
		OutContext.ObstacleBottomZ = SpawnPlan.WorldLocation.Z;
		OutContext.bCanClimbOver = (ObstacleType == EExObstacleType::Slide);

		return true;
	}

	return false;
}

void UExObstacleManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UExObstacleManager, ReplicatedObstacles, Params);
}

void UExObstacleManager::OnRep_ReplicatedObstacles()
{
	// 클라이언트에서 리플리케이트된 배열이 변경될 때 호출됨
	if (!GetOwner() || GetOwner()->HasAuthority()) return;

	UExChunkSpawner* Spawner = GetOwner()->FindComponentByClass<UExChunkSpawner>();
	if (!Spawner) return;

	for (const FExObstacleSyncInfo& Info : ReplicatedObstacles)
	{
		if (Info.Obstacle && Info.SegmentIndex >= 0)
		{
			// 아직 어떤 청크에도 Attach 되지 않은 장애물만 처리
			if (Info.Obstacle->GetAttachParentActor() == nullptr)
			{
				AExFloorChunk* TargetChunk = nullptr;
				for (AExFloorChunk* Chunk : Spawner->GetActiveChunks())
				{
					if (Chunk && Chunk->SegmentIndex == Info.SegmentIndex)
					{
						TargetChunk = Chunk;
						break;
					}
				}

				if (TargetChunk)
				{
					Info.Obstacle->AttachToActor(TargetChunk, FAttachmentTransformRules::KeepWorldTransform);
					TArray<TWeakObjectPtr<AActor>>& SegArray = SpawnedBySegment.FindOrAdd(Info.SegmentIndex);
					SegArray.AddUnique(Info.Obstacle);
				}
			}
		}
	}
}

void UExObstacleManager::OnLocalChunkSpawned(AExFloorChunk* SpawnedChunk)
{
	if (!SpawnedChunk) return;

	// 클라이언트가 방금 새 청크를 스폰했으므로, 리플리케이트 되어있으나 아직 Attach를 못한 장애물 확인
	for (const FExObstacleSyncInfo& Info : ReplicatedObstacles)
	{
		if (Info.Obstacle && Info.SegmentIndex == SpawnedChunk->SegmentIndex)
		{
			if (Info.Obstacle->GetAttachParentActor() == nullptr)
			{
				Info.Obstacle->AttachToActor(SpawnedChunk, FAttachmentTransformRules::KeepWorldTransform);
				TArray<TWeakObjectPtr<AActor>>& SegArray = SpawnedBySegment.FindOrAdd(Info.SegmentIndex);
				SegArray.AddUnique(Info.Obstacle);
			}
		}
	}
}


// ── 기존 SpawnObstaclesOnChunk (래퍼로 변경) ──

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
	// 장애물은 Replicated 액터이므로 서버에서만 스폰해야 합니다. 
	// 클라이언트는 서버가 생성한 액터를 복제받게 됩니다.
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		return;
	}

	if (CachedObstacleDefinitions.Num() == 0) return;
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
	float ChunkStartDist = Chunk->PathDistance - (ChunkLength * 0.5f);
	float ChunkEndDist = ChunkStartDist + ChunkLength;

	// 목표 스폰 거리는 이전 장애물 끝점 + 안전 거리
	float TargetSpawnDist = LastObstacleSafeEndDistance + BoundSpawner->RunnerConfig->ObstacleSpawn.MinSafeDistance;

	// 만약 목표 지점이 현재 청크 시작점 이전이라면, 현재 청크 시작점에서부터 스폰 가능
	if (TargetSpawnDist < ChunkStartDist)
	{
		TargetSpawnDist = ChunkStartDist;
	}

	float ObsLen = SelectedDef->MaxSize.X;

	// 실제 스폰 예정 거리가 현재 활성화된 청크 범위를 벗어난다면 이번 청크엔 스폰 생략
	if (TargetSpawnDist + ObsLen > ChunkEndDist) 
	{
		return;
	}

	float ActualSpawnDist = TargetSpawnDist;

	// ── Strategy 위임: 스폰 위치 계산 ──
	// 거리(PathDistance)로 ActualSpawnDist 전달
	FTransform SpawnTrans = Strategy->CalculateSpawnPosition(SelectedDef, Chunk, ActualSpawnDist);
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
	if (CachedObstacleDefinitions.Num() == 0) return nullptr;

	const int32 Index = FMath::RandRange(0, CachedObstacleDefinitions.Num() - 1);
	return CachedObstacleDefinitions[Index];
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
		AActor* Spawned = GetWorld()->SpawnActor<AActor>(ObstacleClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		if (Spawned)
		{
			Spawned->SetReplicates(true);
			Spawned->SetReplicatingMovement(true);
		}
		return Spawned;
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
				PooledActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
				ActivateObstacle(PooledActor);
				return PooledActor;
			}
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* NewActor = GetWorld()->SpawnActor<AActor>(ObstacleClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (NewActor)
	{
		NewActor->SetReplicates(true);
		NewActor->SetReplicatingMovement(true);
	}
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

			// 장애물 Definition 매칭 (CachedObstacleDefinitions에서 클래스로 역참조)
			EExObstacleType FoundType = EExObstacleType::None;
			bool bFoundClimbable = false;
			float ClimbableThreshold = 200.f;

			for (const UExObstacleDefinition* Def : CachedObstacleDefinitions)
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

