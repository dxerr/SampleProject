// Copyright ExFrameWork. All Rights Reserved.

#include "ExChunkSpawner.h"
#include "../Actors/ExFloorChunk.h"
#include "../GameModes/ExCoreGameMode.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogExChunkSpawner, Log, All);

UExChunkSpawner::UExChunkSpawner()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UExChunkSpawner::BeginPlay()
{
	Super::BeginPlay();
}

void UExChunkSpawner::InitializeSpawner()
{
	UE_LOG(LogExChunkSpawner, Warning, TEXT(">> InitializeSpawner Started"));

	if (!ChunkClass)
	{
		UE_LOG(LogExChunkSpawner, Error, TEXT("ChunkClass is not set!"));
		return;
	}

	// 1. 기존 청크 정리 (Destroy All)
	// 레벨에 배치된 기존 청크와 스포너가 만든 청크 모두 제거하여 상태 초기화
	ClearAllChunks(); 

	UWorld* World = GetWorld();
	if (World)
	{
		// 이전에 레벨에 배치되어 있었지만 스포너 관리 목록에 없던 청크들도 찾아서 제거
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsOfClass(World, AExFloorChunk::StaticClass(), FoundActors);
		
		for (AActor* Actor : FoundActors)
		{
			// 스포너가 소유하지 않았거나 목록에 없던 녀석들도 가차없이 제거
			if (IsValid(Actor))
			{
				UE_LOG(LogExChunkSpawner, Log, TEXT("Destroying existing legacy chunk: %s"), *Actor->GetName());
				Actor->Destroy();
			}
		}
	}

	// 2. 초기 상태 리셋
	ActiveChunks.Empty();
	NextSpawnX = SpawnStartX;

	// 3. MaxActiveChunks 만큼 새로운 BP 청크 생성 (Pre-fill)
	UE_LOG(LogExChunkSpawner, Log, TEXT("Initializing track with %d new chunks from scratch..."), MaxActiveChunks);
	for (int32 i = 0; i < MaxActiveChunks; ++i)
	{
		SpawnNextChunk();
	}

	// 4. 풀에 여유분(Reserve) 미리 생성 (동일 프레임 재사용 방지)
	int32 ReserveCount = 3;
	UE_LOG(LogExChunkSpawner, Log, TEXT("Creating %d reserve chunks in pool..."), ReserveCount);
	for (int32 i = 0; i < ReserveCount; ++i)
	{
		AExFloorChunk* ReserveChunk = CreateNewChunk();
		if (ReserveChunk)
		{
			ReserveChunk->bIsPooled = true;
			// 초기 생성 위치는 지하로
			ReserveChunk->SetActorLocation(FVector(0.f, 0.f, -10000.f));
			ChunkPool.Add(ReserveChunk);
		}
	}

	UE_LOG(LogExChunkSpawner, Log, TEXT("Spawner initialized. Track ends at %.2f"), NextSpawnX);
}

AExFloorChunk* UExChunkSpawner::SpawnNextChunk()
{
	AExFloorChunk* Chunk = GetChunkFromPool();
	
	if (!Chunk)
	{
		UE_LOG(LogExChunkSpawner, Error, TEXT("Failed to get chunk from pool"));
		return nullptr;
	}

	// 1. 머리 청크(가장 앞선 청크) 찾기
	float MaxX = -FLT_MAX;
	bool bFoundActiveParams = false;

	if (ActiveChunks.Num() > 0)
	{
		for (AExFloorChunk* ActiveChunk : ActiveChunks)
		{
			if (IsValid(ActiveChunk) && !ActiveChunk->bIsPooled) 
			{
				// 실제 보이는 메쉬의 Bound 끝부분을 찾아야 함
				FVector Origin, BoxExtent;
				ActiveChunk->GetActorBounds(false, Origin, BoxExtent);
				
				// Origin.X + BoxExtent.X가 이 청크의 끝 지점(Head)
				float ChunkHeadX = Origin.X + BoxExtent.X;
				
				if (ChunkHeadX > MaxX)
				{
					MaxX = ChunkHeadX;
					bFoundActiveParams = true;
				}
			}
		}
	}

	// 2. 스폰 위치 결정
	float SpawnX = SpawnStartX;
	if (bFoundActiveParams)
	{
		// 이전 청크의 끝 지점에서 시작 (빈틈없이 연결)
		// 새로 만들 청크의 길이(HalfSize)를 알아야 중심점(SpawnX)을 잡을 수 있는데...
		// 문제는 아직 스폰을 안 해서 길이를 모름.
		// 하지만 보통 바닥 청크의 Pivot이 중앙(0,0,0)이라면:
		// SpawnX(중심) = MaxX(이전 끝) + NewChunkHalfLength
		
		// 일단 임시 위치(0,0,0)에 먼저 배치 후 Bounds 측정 -> 재배치 전략 사용
		SpawnX = 0.f; 
	}
	else
	{
		// 첫 청크인 경우
		SpawnX = NextSpawnX;
	}

	// 3. 임시 배치 및 활성화 (Bounds 측정을 위해)
	// 충돌 방지를 위해 안전한 지하 공간(-10000)에서 측정
	Chunk->ActivateChunk(FVector(0.f, 0.f, -10000.f));

	// 4. 실제 크기 측정 및 최종 위치 보정
	FVector NewOrigin, NewExtent;
	Chunk->GetActorBounds(false, NewOrigin, NewExtent);
	float NewChunkHalfLength = NewExtent.X;
	float NewChunkLength = NewChunkHalfLength * 2.0f;

	// 최종 위치 계산
	if (bFoundActiveParams)
	{
		// 이전 끝 지점(MaxX) + 내 절반 길이(NewChunkHalfLength) = 내 중심 좌표
		SpawnX = MaxX + NewChunkHalfLength;
	}
	else if (ActiveChunks.Num() == 0) // 진짜 첫 청크
	{
		// SpawnStartX가 트랙의 시작점(꼬리)이라면:
		SpawnX = SpawnStartX + NewChunkHalfLength;
	}
	else 
	{
		// 에러 상황 등에서는 NextSpawnX 사용하되 길이 고려
		SpawnX = NextSpawnX;
	}

	// 5. 최종 위치로 이동
	Chunk->SetActorLocation(FVector(SpawnX, 0.f, 0.f));

	// 델리게이트 연결
	if (!Chunk->OnChunkReachedKillZ.IsAlreadyBound(this, &UExChunkSpawner::OnChunkReachedKillZ))
	{
		Chunk->OnChunkReachedKillZ.AddDynamic(this, &UExChunkSpawner::OnChunkReachedKillZ);
	}

	// 활성 목록에 추가
	if (!ActiveChunks.Contains(Chunk))
	{
		ActiveChunks.Add(Chunk);
	}

	// NextSpawnX 갱신 (다음 청크를 위해 내 끝 지점을 저장해둘 수도 있지만, 위 로직은 매번 MaxX를 찾으므로 단순 참고용으로 저장)
	// 내 끝 지점 = Center + HalfLength
	NextSpawnX = SpawnX + NewChunkHalfLength;

	UE_LOG(LogExChunkSpawner, Verbose, TEXT("Spawned chunk at X=%.2f (Length=%.2f)"), SpawnX, NewChunkLength);

	return Chunk;
}

void UExChunkSpawner::ReturnChunkToPool(AExFloorChunk* Chunk)
{
	if (!Chunk) return;

	UE_LOG(LogExChunkSpawner, Log, TEXT(">> ReturnChunkToPool: %s"), *Chunk->GetName());

	// 활성 목록에서 제거
	ActiveChunks.Remove(Chunk);

	// 풀에 추가
	Chunk->ReturnToPool(); // 액터의 ReturnToPool -> DeactivateChunk 호출
	
	// 중복 추가 방지
	if (!ChunkPool.Contains(Chunk))
	{
		ChunkPool.Add(Chunk);
	}

	UE_LOG(LogExChunkSpawner, Verbose, TEXT("Chunk returned to pool. Active: %d, Pool: %d"), 
		ActiveChunks.Num(), ChunkPool.Num());
}

void UExChunkSpawner::ClearAllChunks()
{
	UE_LOG(LogExChunkSpawner, Warning, TEXT(">> ClearAllChunks Called! (Active: %d)"), ActiveChunks.Num());

	// 활성 청크 정리
	for (AExFloorChunk* Chunk : ActiveChunks)
	{
		if (IsValid(Chunk))
		{
			Chunk->ReturnToPool();
		}
	}
	ActiveChunks.Empty();

	// 풀 청크 정리
	for (AExFloorChunk* Chunk : ChunkPool)
	{
		if (Chunk)
		{
			Chunk->Destroy();
		}
	}
	ChunkPool.Empty();

	NextSpawnX = SpawnStartX;

	UE_LOG(LogExChunkSpawner, Log, TEXT("All chunks cleared"));
}

void UExChunkSpawner::ShiftWorld(float DeltaX)
{
	// 모든 활성 청크를 DeltaX만큼 이동
	for (AExFloorChunk* Chunk : ActiveChunks)
	{
		if (IsValid(Chunk))
		{
			Chunk->AddActorWorldOffset(FVector(DeltaX, 0.f, 0.f), false, nullptr, ETeleportType::None);
		}
	}

	// NextSpawnX도 같이 이동해야 스폰 위치가 유지됨
	NextSpawnX += DeltaX;
}

void UExChunkSpawner::OnChunkReachedKillZ(AExFloorChunk* Chunk)
{
	if (!Chunk) return;

	UE_LOG(LogExChunkSpawner, Log, TEXT(">> OnChunkReachedKillZ: %s reached limit"), *Chunk->GetName());

	// 청크를 풀로 반환
	ReturnChunkToPool(Chunk);

	// 새 청크 스폰
	SpawnNextChunk();
}

AExFloorChunk* UExChunkSpawner::GetChunkFromPool()
{
	if (ChunkPool.Num() > 0)
	{
		// LIFO(Stack) 대신 FIFO(Queue) 방식으로 변경하여 방금 반환된 청크가 즉시 재사용되는 것 방지
		// return ChunkPool.Pop();
		
		AExFloorChunk* Chunk = ChunkPool[0];
		ChunkPool.RemoveAt(0); // 가장 오래된 놈 꺼내기
		return Chunk;
	}

	// 풀이 비었으면 새로 생성
	return CreateNewChunk();
}

AExFloorChunk* UExChunkSpawner::CreateNewChunk()
{
	UWorld* World = GetWorld();
	if (!World || !ChunkClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = GetOwner();

	AExFloorChunk* NewChunk = World->SpawnActor<AExFloorChunk>(
		ChunkClass,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams
	);

	if (NewChunk)
	{
		// NewChunk->DeactivateChunk(); // 생성 직후 숨기면 렌더링 꼬임 발생 가능.
		NewChunk->bIsPooled = true; // 풀링 상태만 설정 (어차피 바로 Activate됨)
		UE_LOG(LogExChunkSpawner, Log, TEXT("Created new chunk: %s"), *NewChunk->GetName());
	}

	return NewChunk;
}
