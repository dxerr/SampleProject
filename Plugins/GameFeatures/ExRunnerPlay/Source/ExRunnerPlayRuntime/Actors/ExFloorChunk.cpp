// Copyright ExFrameWork. All Rights Reserved.

#include "ExFloorChunk.h"
#include "../GameModes/ExRunnerGameMode.h"
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

FBox AExFloorChunk::GetFloorBounds() const
{
	if (FloorMesh)
	{
		return FloorMesh->Bounds.GetBox();
	}
	return FBox(FVector(-500,-500,-10), FVector(500,500,10));
}

void AExFloorChunk::BeginPlay()
{
	Super::BeginPlay();

	// 게임모드 캐시
	CachedGameMode = Cast<AExRunnerGameMode>(UGameplayStatics::GetGameMode(this));
	
	if (!CachedGameMode)
	{
		UE_LOG(LogExFloorChunk, Warning, TEXT("ExFloorChunk: Could not find ExRunnerGameMode"));
	}
	else
	{
		UE_LOG(LogExFloorChunk, Log, TEXT("ExFloorChunk: GameMode found, CurrentSpeed=%.2f"), CachedGameMode->GetCurrentTreadmillSpeed());
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

	if (bIsPooled)
	{
		return;
	}

	// [위치 변화량 기반 트레드밀]
	// Floor 이동은 GameMode::Tick → ChunkSpawner::ShiftWorld에서 처리
	// FloorChunk는 KillZ 도달 체크만 수행

	// KillZ 도달 체크
	if (GetActorLocation().X < KillZ)
	{
		UE_LOG(LogExFloorChunk, Verbose, TEXT("Chunk %s reached KillZ"), *GetName());
		
		// 델리게이트 브로드캐스트 (스포너에서 처리)
		// 스포너가 ReturnChunkToPool을 호출하므로, 여기서 중복 호출하지 않음
		if (OnChunkReachedKillZ.IsBound())
		{
			OnChunkReachedKillZ.Broadcast(this);
		}
		else
		{
			// 바인딩된 스포너가 없는 경우에만 스스로 처리 (안전장치)
			ReturnToPool();
		}
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
	
	// 렌더 상태 강제 갱신
	MarkComponentsRenderStateDirty();

	UE_LOG(LogExFloorChunk, Warning, TEXT("[%s] ActivateChunk Called. Loc: %s, IsHidden: %d"), 
		*GetName(), *SpawnLocation.ToString(), IsHidden());
	if (FloorMesh)
	{
		UE_LOG(LogExFloorChunk, Warning, TEXT("  - Mesh HiddenInGame: %d, Visible: %d"), 
			FloorMesh->bHiddenInGame, FloorMesh->GetVisibleFlag());
	}
}

void AExFloorChunk::DeactivateChunk()
{
	bIsPooled = true;

	// Gap 적용 중이면 원상복구
	ClearGap();
	
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

// ──────────────────────────────────────────────
// Gap 적용: FloorMesh 숨기고 양쪽 바닥 조각 생성
// ──────────────────────────────────────────────
void AExFloorChunk::ApplyGap(float GapLocalStartX, float GapWidth)
{
	// 이미 Gap 적용 중이면 먼저 해제
	if (bHasGap) ClearGap();
	if (!FloorMesh || !FloorMesh->GetStaticMesh()) return;

	// 원본 메시/머티리얼 참조
	UStaticMesh* OrigMesh = FloorMesh->GetStaticMesh();
	const float HalfLen = ChunkLength * 0.5f;

	// Gap 경계 (로컬 좌표, 청크 중심 = 0)
	const float GapStartX = GapLocalStartX;
	const float GapEndX   = GapLocalStartX + GapWidth;

	UE_LOG(LogExFloorChunk, Log, TEXT("[%s] ApplyGap: LocalX=%.1f, Width=%.1f (HalfLen=%.1f)"),
		*GetName(), GapLocalStartX, GapWidth, HalfLen);

	// ── 1. 원본 FloorMesh 숨기기 ──
	FloorMesh->SetVisibility(false, true);
	FloorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// ── 2. 원본 메시의 로컬 바운드로 기본 크기 계산 ──
	FBoxSphereBounds MeshBounds = OrigMesh->GetBounds();
	FVector MeshExtent = MeshBounds.BoxExtent; // 메시 원본 반크기
	FVector MeshOrigin = MeshBounds.Origin;

	// 원본 FloorMesh의 현재 스케일을 고려
	FVector FloorScale = FloorMesh->GetRelativeScale3D();
	float OrigMeshLenX = MeshExtent.X * 2.0f * FloorScale.X; // 실제 월드 X 길이

	// ── 3. 왼쪽 바닥 조각 (ChunkStart ~ GapStart) ──
	float LeftLen = GapStartX - (-HalfLen); // 왼쪽 조각 길이
	if (LeftLen > 1.f)
	{
		UStaticMeshComponent* LeftFloor = NewObject<UStaticMeshComponent>(this, 
			UStaticMeshComponent::StaticClass(), FName(TEXT("GapFloor_Left")));
		LeftFloor->SetStaticMesh(OrigMesh);

		// 원본 머티리얼 복사
		for (int32 i = 0; i < FloorMesh->GetNumMaterials(); ++i)
		{
			LeftFloor->SetMaterial(i, FloorMesh->GetMaterial(i));
		}

		LeftFloor->SetCollisionProfileName(TEXT("BlockAll"));
		LeftFloor->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
		LeftFloor->RegisterComponent();

		// ★ 스케일: 부모(FloorMesh)가 이미 FloorScale을 가지고 있으므로
		//    자식의 RelativeScale은 비율만 설정 (Y/Z = 1, 부모에서 상속)
		float LeftScaleX = LeftLen / OrigMeshLenX;
		LeftFloor->SetRelativeScale3D(FVector(LeftScaleX, 1.f, 1.f));

		// ★ 위치: RelativeLocation은 부모 스케일로 곱해지므로
		//    월드 좌표를 부모 스케일로 나눠서 로컬 좌표 변환
		float LeftCenterX = (-HalfLen) + (LeftLen * 0.5f);
		LeftFloor->SetRelativeLocation(FVector(LeftCenterX / FloorScale.X, 0.f, 0.f));

		GapFloorPieces.Add(LeftFloor);
	}

	// ── 4. 오른쪽 바닥 조각 (GapEnd ~ ChunkEnd) ──
	float RightLen = HalfLen - GapEndX; // 오른쪽 조각 길이
	if (RightLen > 1.f)
	{
		UStaticMeshComponent* RightFloor = NewObject<UStaticMeshComponent>(this,
			UStaticMeshComponent::StaticClass(), FName(TEXT("GapFloor_Right")));
		RightFloor->SetStaticMesh(OrigMesh);

		// 원본 머티리얼 복사
		for (int32 i = 0; i < FloorMesh->GetNumMaterials(); ++i)
		{
			RightFloor->SetMaterial(i, FloorMesh->GetMaterial(i));
		}

		RightFloor->SetCollisionProfileName(TEXT("BlockAll"));
		RightFloor->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
		RightFloor->RegisterComponent();

		// ★ 스케일: 비율만 설정 (부모 FloorScale 자동 적용)
		float RightScaleX = RightLen / OrigMeshLenX;
		RightFloor->SetRelativeScale3D(FVector(RightScaleX, 1.f, 1.f));

		// ★ 위치: 월드 → 부모 로컬 좌표 변환
		float RightCenterX = GapEndX + (RightLen * 0.5f);
		RightFloor->SetRelativeLocation(FVector(RightCenterX / FloorScale.X, 0.f, 0.f));

		GapFloorPieces.Add(RightFloor);
	}

	bHasGap = true;
}

// ──────────────────────────────────────────────
// Gap 해제: 바닥 조각 파괴, FloorMesh 복원
// ──────────────────────────────────────────────
void AExFloorChunk::ClearGap()
{
	if (!bHasGap) return;

	// 동적 생성된 바닥 조각 제거
	for (UStaticMeshComponent* Piece : GapFloorPieces)
	{
		if (Piece)
		{
			Piece->DestroyComponent();
		}
	}
	GapFloorPieces.Empty();

	// 원본 FloorMesh 복원
	if (FloorMesh)
	{
		FloorMesh->SetVisibility(true, true);
		FloorMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	bHasGap = false;

	UE_LOG(LogExFloorChunk, Log, TEXT("[%s] ClearGap: FloorMesh restored"), *GetName());
}
