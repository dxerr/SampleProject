// Copyright ExFrameWork. All Rights Reserved.

#include "ExFloorChunk.h"
#include "../GameModes/ExCoreGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogExFloorChunk, Log, All);

AExFloorChunk::AExFloorChunk()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 루트 컴포넌트로 바닥 메시 생성
	FloorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FloorMesh"));
	RootComponent = FloorMesh;

	// 기본 콜리전 설정
	FloorMesh->SetCollisionProfileName(TEXT("BlockAll"));
}

void AExFloorChunk::BeginPlay()
{
	Super::BeginPlay();

	// 게임모드 캐시
	CachedGameMode = Cast<AExCoreGameMode>(UGameplayStatics::GetGameMode(this));
	
	if (!CachedGameMode)
	{
		UE_LOG(LogExFloorChunk, Warning, TEXT("ExFloorChunk: Could not find ExCoreGameMode"));
	}
	else
	{
		UE_LOG(LogExFloorChunk, Log, TEXT("ExFloorChunk: GameMode found, CurrentSpeed=%.2f"), CachedGameMode->GetCurrentGameSpeed());
	}

	// 레벨에 직접 배치된 청크는 자동으로 활성화
	if (!bIsPooled)
	{
		SetActorTickEnabled(true);
		UE_LOG(LogExFloorChunk, Log, TEXT("ExFloorChunk %s: Tick enabled on BeginPlay"), *GetName());
	}
}

void AExFloorChunk::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!CachedGameMode || bIsPooled)
	{
		return;
	}

	// 게임모드에서 현재 속도 가져오기
	const float GameSpeed = CachedGameMode->GetCurrentGameSpeed();
	
	if (GameSpeed <= 0.f)
	{
		return;
	}

	// World Shift 방식으로 변경됨에 따라, 더 이상 FloorChunk 스스로 이동하지 않음
	// float ClampedDeltaTime = FMath::Min(DeltaTime, 0.1f);
	// FVector Offset(-GameSpeed * ClampedDeltaTime, 0.f, 0.f);
	// AddActorWorldOffset(Offset, false, nullptr, ETeleportType::None);

	// KillZ 도달 체크
	if (GetActorLocation().X < KillZ)
	{
		UE_LOG(LogExFloorChunk, Verbose, TEXT("Chunk %s reached KillZ"), *GetName());
		
		// 델리게이트 브로드캐스트 (스포너에서 처리)
		OnChunkReachedKillZ.Broadcast(this);
		
		// 풀로 반환
		ReturnToPool();
	}
}

void AExFloorChunk::ActivateChunk(const FVector& SpawnLocation)
{
	bIsPooled = false;
	
	SetActorLocation(SpawnLocation);
	
	// 액터 및 메시 가시성 강제 활성화
	SetActorHiddenInGame(false);
	if (FloorMesh)
	{
		FloorMesh->SetHiddenInGame(false);
		FloorMesh->SetVisibility(true, true); // Propagate to children
	}

	SetActorEnableCollision(true);
	SetActorTickEnabled(true);
	
	// 렌더 상태 강제 갱신 (한 프레임 내 변경 시 씹힘 방지)
	MarkComponentsRenderStateDirty();

	UE_LOG(LogExFloorChunk, Log, TEXT("[%s] Chunk activated at %s"), *GetName(), *SpawnLocation.ToString());
}

void AExFloorChunk::DeactivateChunk()
{
	bIsPooled = true;
	
	SetActorHiddenInGame(true);
	if (FloorMesh)
	{
		FloorMesh->SetHiddenInGame(true);
		FloorMesh->SetVisibility(false, true);
	}

	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
	
	// 렌더 상태 강제 갱신
	MarkComponentsRenderStateDirty();

	// 위치는 변경하지 않음 (SpawnNextChunk에서 어차피 재설정됨)
	// 좌표 오염 방지

	UE_LOG(LogExFloorChunk, Log, TEXT("[%s] Chunk deactivated at %s"), *GetName(), *GetActorLocation().ToString());
}

void AExFloorChunk::ReturnToPool()
{
	DeactivateChunk();
	
	// 추가 처리는 OnChunkReachedKillZ 델리게이트를 통해 스포너에서 수행
}
