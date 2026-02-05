


// Copyright ExFrameWork. All Rights Reserved.

#include "ExChunkSpawner.h"
#include "../Actors/ExFloorChunk.h"
#include "Kismet/GameplayStatics.h"

// ... existing code ...

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/BoxComponent.h" // For InteractionComponent Setup

DEFINE_LOG_CATEGORY_STATIC(LogExChunkSpawner, Log, All);

UExChunkSpawner::UExChunkSpawner()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// --- 청크 스포너 구현 ---

void UExChunkSpawner::BeginPlay()
{
	Super::BeginPlay();
	
	// 초기 풀 크기만큼 청크 생성 보관
	for (int32 i = 0; i < InitialPoolSize; ++i)
	{
		AExFloorChunk* NewChunk = CreateNewChunk();
		if (NewChunk)
		{
			ReturnChunkToPool(NewChunk);
		}
	}
	
	// 초기 스폰 위치 설정
	NextSpawnX = SpawnStartX;
}

void UExChunkSpawner::InitializeSpawner()
{
	// 기존 청크 모두 정리
	ClearAllChunks();
	
	// 초기 청크 배치
	for (int32 i = 0; i < MaxActiveChunks; ++i)
	{
		SpawnNextChunk();
	}
}

AExFloorChunk* UExChunkSpawner::SpawnNextChunk()
{
	AExFloorChunk* Chunk = GetChunkFromPool();
	if (!Chunk)
	{
		return nullptr;
	}
	
	// 스폰 위치 계산 (마지막 청크 기준)
	float SpawnX = SpawnStartX;
	if (ActiveChunks.Num() > 0)
	{
		AExFloorChunk* LastChunk = ActiveChunks.Last();
		if (IsValid(LastChunk))
		{
			SpawnX = LastChunk->GetActorLocation().X + ChunkSpacing;
		}
	}
	// 만약 활성 청크가 없다면 SpawnStartX(0)부터 시작 (혹은 NextSpawnX 유지)
	// 여기서는 Reset 개념으로 SpawnStartX 사용 혹은 기존 NextSpawnX 사용 가능
	// 하지만 Treadmill 특성상 "이어지는" 것이 중요하므로 LastChunk 기반이 확실함.
	
	// 청크 활성화 (ActivateChunk 메서드 사용으로 메시 가시성 등 초기화 보장)
	FVector SpawnLocation(SpawnX, 0.f, 0.f);
	Chunk->ActivateChunk(SpawnLocation);
	
	// 활성 목록에 추가
	ActiveChunks.Add(Chunk);
	
	// NextSpawnX 변수는 이제 "다음"을 위해 누적할 필요 없이, 
	// 항상 리스트 기반으로 계산하므로 업데이트 로직 제거
	// NextSpawnX += ChunkSpacing; (Removed)
	
	// 델리게이트를 통해 청크 생성 알림 (장애물 배치 등)
	if (OnChunkSpawned.IsBound())
	{
		OnChunkSpawned.Broadcast(Chunk);
	}
	
	return Chunk;
}

void UExChunkSpawner::ReturnChunkToPool(AExFloorChunk* Chunk)
{
	if (!IsValid(Chunk))
	{
		return;
	}
	
	// 청크 비활성화 (DeactivateChunk 메서드 사용)
	Chunk->DeactivateChunk();
	
	// 활성 목록에서 제거
	ActiveChunks.Remove(Chunk);
	
	// 소멸 알림 (장애물 정리 등)
	if (OnChunkDespawned.IsBound())
	{
		OnChunkDespawned.Broadcast(Chunk);
	}

	// 풀에 반환
	ChunkPool.Add(Chunk);
}

void UExChunkSpawner::ClearAllChunks()
{
	for (AExFloorChunk* Chunk : ActiveChunks)
	{
		if (IsValid(Chunk))
		{
			ReturnChunkToPool(Chunk);
		}
	}
	ActiveChunks.Empty();
}

void UExChunkSpawner::ShiftWorld(float DeltaX)
{
	for (AExFloorChunk* Chunk : ActiveChunks)
	{
		if (IsValid(Chunk))
		{
			FVector CurrentLocation = Chunk->GetActorLocation();
			CurrentLocation.X += DeltaX;
			Chunk->SetActorLocation(CurrentLocation);
		}
	}
	
	// 스폰 위치 업데이트 (더 이상 사용 안함 - Relative 방식)
	// NextSpawnX += DeltaX;

	// 외부 시스템(장애물 매니저 등)에 시프트 알림
	if (OnWorldShifted.IsBound())
	{
		OnWorldShifted.Broadcast(DeltaX);
	}
}

void UExChunkSpawner::OnChunkReachedKillZ(AExFloorChunk* Chunk)
{
	if (IsValid(Chunk))
	{
		// 청크를 풀로 반환
		ReturnChunkToPool(Chunk);
		
		// 무한 맵 유지를 위해 새 청크 생성
		SpawnNextChunk();
	}
}

AExFloorChunk* UExChunkSpawner::GetChunkFromPool()
{
	// FIFO (First-In, First-Out) 방식으로 변경
	// 가장 오래된(먼저 들어온) 청크를 재사용하여, 렌더링/물리 상태 안정화 시간 확보
	if (ChunkPool.Num() > 0)
	{
		// 0번 인덱스(가장 오래된 녀석) 가져오기
		AExFloorChunk* Chunk = ChunkPool[0];
		ChunkPool.RemoveAt(0); // 앞쪽 제거 (Shift 발생하지만 풀 크기가 작으므로 무시 가능)
		
		if (IsValid(Chunk))
		{
			return Chunk;
		}
	}
	
	// 풀이 비었으면 새로 생성
	return CreateNewChunk();
}

AExFloorChunk* UExChunkSpawner::CreateNewChunk()
{
	if (!ChunkClass)
	{
		UE_LOG(LogExChunkSpawner, Error, TEXT("ChunkClass is not set!"));
		return nullptr;
	}
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	AExFloorChunk* NewChunk = GetWorld()->SpawnActor<AExFloorChunk>(ChunkClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (NewChunk)
	{
		// KillZ 도달 델리게이트 바인딩
		NewChunk->OnChunkReachedKillZ.AddDynamic(this, &UExChunkSpawner::OnChunkReachedKillZ);
	}
	
	return NewChunk;
}

// --- Obstacle System Implementations (Moved to ExObstacleManager) ---
// See ExObstacleManager.cpp for implementation details.

