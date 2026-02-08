/**
 * @file ExChunkSpawner.cpp
 * @brief 청크 스폰 및 오브젝트 풀 관리 컴포넌트 구현
 * @details 러너 게임에서 무한 맵 생성을 위한 청크 풀링 시스템
 * 
 * Copyright ExFrameWork. All Rights Reserved.
 */

#include "ExChunkSpawner.h"
#include "../Actors/ExFloorChunk.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/BoxComponent.h"

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
	// 서버 권한 체크 (멀티플레이어 환경 고려)
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		return nullptr;
	}

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
	
	// 청크 활성화
	FVector SpawnLocation(SpawnX, 0.f, 0.f);
	Chunk->ActivateChunk(SpawnLocation);
	
	// 활성 목록에 추가
	ActiveChunks.Add(Chunk);
	
	// 델리게이트를 통해 청크 생성 알림
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
	
	// 서버 권한 체크 (멀티플레이어 환경 고려)
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		return;
	}

	// 청크 비활성화
	Chunk->DeactivateChunk();
	
	// 활성 목록에서 제거
	ActiveChunks.Remove(Chunk);
	
	// 소멸 알림
	if (OnChunkDespawned.IsBound())
	{
		OnChunkDespawned.Broadcast(Chunk);
	}

	// 풀에 반환 (앞에 삽입 - FIFO 방식)
	// Pop()은 뒤에서 꺼내므로, 앞에 삽입하면 최소 1사이클 딜레이 확보
	// 이를 통해 방금 반환된 청크가 즉시 재사용되는 것을 방지
	ChunkPool.Insert(Chunk, 0);
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
	// 서버 권한 체크
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		return;
	}

	for (AExFloorChunk* Chunk : ActiveChunks)
	{
		if (IsValid(Chunk))
		{
			FVector CurrentLocation = Chunk->GetActorLocation();
			CurrentLocation.X += DeltaX;
			Chunk->SetActorLocation(CurrentLocation);
		}
	}

	// 외부 시스템에 시프트 알림
	if (OnWorldShifted.IsBound())
	{
		OnWorldShifted.Broadcast(DeltaX);
	}
}

void UExChunkSpawner::OnChunkReachedKillZ(AExFloorChunk* Chunk)
{
	if (IsValid(Chunk))
	{
		// [Fix] 순서 변경: 먼저 스폰 → 그 다음 반환
		// 이렇게 하면 삭제된 청크가 즉시 재사용되는 것을 방지
		
		// 1️⃣ 먼저 새 청크 스폰 (풀에서 다른 청크 사용)
		SpawnNextChunk();
		
		// 2️⃣ 그 다음 삭제된 청크를 풀로 반환
		ReturnChunkToPool(Chunk);
	}
}

AExFloorChunk* UExChunkSpawner::GetChunkFromPool()
{
	// FIFO (First-In, First-Out) 방식
	// Insert(0)으로 앞에 추가, Pop()으로 뒤에서 제거
	// 이를 통해 반환된 청크가 최소 1사이클 이상 대기 후 재사용됨
	if (ChunkPool.Num() > 0)
	{
		AExFloorChunk* Chunk = ChunkPool.Pop();
		
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

