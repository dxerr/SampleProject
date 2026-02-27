// Copyright ExFrameWork. All Rights Reserved.

#include "ExFloorChunk.h"
#include "../GameModes/ExRunnerGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "ExDebugStateSubsystem.h"
#include "ExGameplayTags.h"
#include "Engine/GameInstance.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogExFloorChunk, Log, All);

AExFloorChunk::AExFloorChunk()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 루트 컴포넌트 생성 (스케일 왜곡 방지용)
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// 바닥 메시 생성 및 루트에 부착
	FloorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FloorMesh"));
	FloorMesh->SetupAttachment(RootComponent);

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
		UE_LOG(LogExFloorChunk, Log, TEXT("ExFloorChunk: GameMode found."));
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
	// Floor 이동은 GameMode::Tick → ChunkSpawner에서 처리
	// FloorChunk는 KillZ 도달 체크만 수행

	// KillZ 도달 체크 (경로 거리 기반 또는 상대 X좌표 기반)
	bool bReachedKillZ = false;

	if (CachedGameMode)
	{
		// 경로 기반 삭제 로직. 트레드밀이 아닌 캐릭터 주행 기반 시스템.
		float PlayerDist = CachedGameMode->GetPlayerPathDistance();
		// KillZ는 스폰 오프셋(음수값)으로, 캐릭터 뒤쪽 범위를 의미
		bReachedKillZ = (PathDistance < PlayerDist + KillZ);
	}

	if (bReachedKillZ)
	{
		UE_LOG(LogExFloorChunk, Verbose, TEXT("Chunk %s reached KillZ (PathDist=%.1f, X=%.1f)"), 
			*GetName(), PathDistance, GetActorLocation().X);
		
		// 델리게이트 브로드캐스트 (스포너에서 처리)
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

	// ──────────────────────────────────────────────
	// 디버그 시각화 (TAG_Ex_Debug_Chunk)
	// ──────────────────────────────────────────────
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UExDebugStateSubsystem* DS = GI->GetSubsystem<UExDebugStateSubsystem>())
			{
				if (DS->IsCheatEnabled(TAG_Ex_Debug_Chunk))
				{
					// --- 바운드 그리기 (실제 경로 곡률 반영) ---
					FBox BaseBounds = GetFloorBounds();
					float FloorExtY = BaseBounds.GetExtent().Y; // 보통 50 * Scale
					float HalfWidth = FloorExtY * FloorMesh->GetRelativeScale3D().Y; // World 기준 폭 절반
					
					TArray<FVector> LeftPoints;
					TArray<FVector> RightPoints;
					
					// 곡선(혹은 직선)을 따라 여러 개의 포인트를 샘플링하여 좌우 가장자리 선형(Polyline) 생성
					int32 SampleCount = (SegmentType == EExPathSegmentType::Straight) ? 2 : 10;
					
					for (int32 i = 0; i <= SampleCount; ++i)
					{
						float Dist = (ChunkLength / SampleCount) * i;
						FTransform LocalTrans = GetLocalTransformAtDistance(Dist);
						
						// 중앙 기준 좌측(-Right), 우측(+Right)
						FVector LeftLocal = LocalTrans.GetLocation() - LocalTrans.GetRotation().GetRightVector() * HalfWidth;
						FVector RightLocal = LocalTrans.GetLocation() + LocalTrans.GetRotation().GetRightVector() * HalfWidth;
						
						LeftPoints.Add(ActorToWorld().TransformPosition(LeftLocal) + FVector(0,0,5.f));
						RightPoints.Add(ActorToWorld().TransformPosition(RightLocal) + FVector(0,0,5.f));
					}
					
					// 테두리 라인 그리기
					for (int32 i = 0; i < SampleCount; ++i)
					{
						DrawDebugLine(World, LeftPoints[i], LeftPoints[i+1], FColor::Green, false, -1.f, 0, 3.f);
						DrawDebugLine(World, RightPoints[i], RightPoints[i+1], FColor::Green, false, -1.f, 0, 3.f);
					}
					// 시작면과 끝면(앞뒤 절단면 캡)
					if (SampleCount > 0)
					{
						DrawDebugLine(World, LeftPoints[0], RightPoints[0], FColor::Green, false, -1.f, 0, 3.f);
						DrawDebugLine(World, LeftPoints.Last(), RightPoints.Last(), FColor::Green, false, -1.f, 0, 3.f);
					}
					
					// 중심 스탯 출력 (중앙선)
					FTransform CenterTrans = GetLocalTransformAtDistance(ChunkLength * 0.5f);
					FVector WorldCenter = ActorToWorld().TransformPosition(CenterTrans.GetLocation());
					FString ChunkDebugStr = FString::Printf(TEXT("Dist: %.0f"), PathDistance);
					DrawDebugString(World, WorldCenter + FVector(0.0f, 0.0f, 50.f), ChunkDebugStr, nullptr, FColor::White, 0.0f, false);
					
					// --- [Gap (구멍) 공간 측정선 그리기] ---
					if (bDebugHasGap && DebugGapStartDist >= 0.f && DebugGapEndDist >= 0.f)
					{
						// Transform 구하기 (구멍 앞면 잘린 곳과 뒷면 잘린 곳의 곡선 로컬 궤도)
						FTransform StartLocal = GetLocalTransformAtDistance(DebugGapStartDist);
						FTransform EndLocal = GetLocalTransformAtDistance(DebugGapEndDist);

						// 각 잘린 단면의 "가운데(Center)" 지점을 월드 좌표로 변환
						FVector WorldStart = ActorToWorld().TransformPosition(StartLocal.GetLocation());
						FVector WorldEnd = ActorToWorld().TransformPosition(EndLocal.GetLocation());
						
						// 실제 공간(두 절단면 사이의 직선/현)의 물리적 길이 계산
						float ActualHoleDistance = FVector::Dist(WorldStart, WorldEnd);
						
						// 눈에 띄도록 포인트와 라인을 약간 위로 리프트(띄움)
						FVector Lift = FVector(0.f, 0.f, 20.f);
						FVector LineStart = WorldStart + Lift;
						FVector LineEnd   = WorldEnd   + Lift;
						FVector LineCenter = (LineStart + LineEnd) * 0.5f;

						DrawDebugLine(World, LineStart, LineEnd, FColor::Red, false, -1.f, 0, 8.f);
						
						// 시작 지점과 끝 지점에 포인터 생성
						DrawDebugPoint(World, LineStart, 15.f, FColor::Orange, false, -1.f, 0);
						DrawDebugPoint(World, LineEnd, 15.f, FColor::Orange, false, -1.f, 0);
						
						// 길이 표시
						FString GapLengthStr = FString::Printf(TEXT("Gap Length: %.1f"), ActualHoleDistance);
						DrawDebugString(World, LineCenter + FVector(0.f, 0.f, 30.f), GapLengthStr, nullptr, FColor::Orange, 0.0f, false);
					}
				}
			}
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
	// 커브 적용 중이면 원상복구
	ClearCurve();
	
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
// Gap 적용: FloorMesh 숨기고 양쪽 바닥 조각 생성 (직선) 또는 SplineMesh 숨김 (곡선)
// ──────────────────────────────────────────────
void AExFloorChunk::ApplyGap(float GapStartDist, float GapWidth)
{
	// 이미 Gap 적용 중이면 먼저 해제
	if (bHasGap) ClearGap();
	if (!FloorMesh || !FloorMesh->GetStaticMesh()) return;

	const float GapStart = GapStartDist;
	const float GapEnd   = GapStartDist + GapWidth;

	UE_LOG(LogExFloorChunk, Log, TEXT("[%s] ApplyGap: StartDist=%.1f, Width=%.1f, isCurve=%d"),
		*GetName(), GapStart, GapWidth, bHasCurve);

	if (SegmentType != EExPathSegmentType::Straight && bHasCurve && CurveSplineMeshes.Num() > 0)
	{
		float ArcSegLen = ChunkLength / CurveSplineMeshes.Num();
		
		int32 StartSegIndex = FMath::RoundToInt(GapStart / ArcSegLen);
		
		for (int32 i = 0; i < CurveSplineMeshes.Num(); ++i)
		{
			float SegStartDist = i * ArcSegLen;
			float SegEndDist = (i + 1) * ArcSegLen;

			// 세그먼트 구간과 [GapStart, GapEnd] 구간이 조금이라도 겹치는지 확인
			if (SegEndDist > GapStart && SegStartDist < GapEnd)
			{
				// 원본 스플라인 조각은 무조건 숨김
				CurveSplineMeshes[i]->SetVisibility(false, true);
				CurveSplineMeshes[i]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				
				// 세그먼트의 앞부분이 Gap구간 전에 걸쳐 살려야 하는 경우, 앞부분 조각 다시 생성
				if (SegStartDist < GapStart)
				{
					SpawnGapSplineMesh(SegStartDist, GapStart, CurveSplineMeshes[i]->GetMaterial(0));
				}
				
				// 세그먼트의 뒷부분이 Gap구간 뒤에 걸쳐 살려야 하는 경우, 뒷부분 조각 다시 생성
				if (SegEndDist > GapEnd)
				{
					SpawnGapSplineMesh(GapEnd, SegEndDist, CurveSplineMeshes[i]->GetMaterial(0));
				}
			}
		}

		// 곡선의 조각 구멍 파임 로직이 이제 완벽히 GapStart ~ GapEnd구간으로 재단되므로 그대로 디버그 변수에 할당
		bDebugHasGap = true;
		DebugGapStartDist = GapStart;
		DebugGapEndDist = GapEnd;
	}
	else
	{
		// === 직선 바닥 처리 ===
		
		bDebugHasGap = true;
		DebugGapStartDist = GapStart;
		DebugGapEndDist = GapEnd;
		
		// 통일성을 위해 ApplyCurve가 불려서 스플라인 메쉬가 생성되어 있더라도
		// 직선 바닥은 완벽한 길이 재단을 위해 통째로 숨깁니다.
		if (bHasCurve)
		{
			for (USplineMeshComponent* Spl : CurveSplineMeshes)
			{
				if (Spl)
				{
					Spl->SetVisibility(false, true);
					Spl->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				}
			}
		}

		// 원본 메시는 통째로 숨기고 Gap 전/후로 잘라진 메쉬 두 개를 생성
		FloorMesh->SetVisibility(false, true);
		FloorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		UStaticMesh* OrigMesh = FloorMesh->GetStaticMesh();
		const float HalfLen = ChunkLength * 0.5f;

		// 피벗이 시작점에 있으므로, 로컬 X좌표(-HalfLen~+HalfLen) 변환은 필요하지 않음.

		FBoxSphereBounds MeshBounds = OrigMesh->GetBounds();
		FVector MeshExtent = MeshBounds.BoxExtent;  // 보통 50 (100짜리 플레인)
		FVector FloorScale = FloorMesh->GetRelativeScale3D(); // 원본 메시 스케일 (예: 10, 4, 1)
		
		// 스케일 되지 않은 원래 메쉬의 X축 순수 길이 (보통 100)
		float OrigMeshBaseLenX = MeshExtent.X * 2.0f; 
		float ActorScaleX = GetActorScale3D().X;
		if (FMath::IsNearlyZero(ActorScaleX)) ActorScaleX = 1.0f;

		const float HalfLength = ChunkLength * 0.5f;

		// ── 왼쪽 바닥 조각 (0 ~ GapStart) ──
		float LeftLen = GapStart;
		if (LeftLen > 1.f)
		{
			UStaticMeshComponent* LeftFloor = NewObject<UStaticMeshComponent>(this, 
				UStaticMeshComponent::StaticClass(), FName(TEXT("GapFloor_Left")));
			LeftFloor->SetStaticMesh(OrigMesh);

			for (int32 i = 0; i < FloorMesh->GetNumMaterials(); ++i) { LeftFloor->SetMaterial(i, FloorMesh->GetMaterial(i)); }

			LeftFloor->SetCollisionProfileName(TEXT("BlockAll"));
			LeftFloor->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
			LeftFloor->RegisterComponent();

			// 새로운 스케일 = 필요한월드길이 / (메쉬순수길이 * 부모액터스케일)
			float FinalScaleX = LeftLen / (OrigMeshBaseLenX * ActorScaleX);
			LeftFloor->SetRelativeScale3D(FVector(FinalScaleX, FloorScale.Y, FloorScale.Z));

			// 기본 Plane 메시는 피벗이 중앙에 있으므로, 길이에 맞춰 중앙 X 위치를 스케일 역보정하여 배치합니다.
			float LeftWorldCenterX = -HalfLength + (LeftLen * 0.5f);
			LeftFloor->SetRelativeLocation(FVector(LeftWorldCenterX / ActorScaleX, FloorMesh->GetRelativeLocation().Y, FloorMesh->GetRelativeLocation().Z));
			GapFloorPieces.Add(LeftFloor);
		}

		// ── 오른쪽 바닥 조각 (GapEnd ~ ChunkEnd) ──
		float RightLen = ChunkLength - GapEnd;
		if (RightLen > 1.f)
		{
			UStaticMeshComponent* RightFloor = NewObject<UStaticMeshComponent>(this,
				UStaticMeshComponent::StaticClass(), FName(TEXT("GapFloor_Right")));
			RightFloor->SetStaticMesh(OrigMesh);

			for (int32 i = 0; i < FloorMesh->GetNumMaterials(); ++i) { RightFloor->SetMaterial(i, FloorMesh->GetMaterial(i)); }

			RightFloor->SetCollisionProfileName(TEXT("BlockAll"));
			RightFloor->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
			RightFloor->RegisterComponent();

			// 새로운 스케일 = 필요한월드길이 / (메쉬순수길이 * 부모액터스케일)
			float FinalScaleX = RightLen / (OrigMeshBaseLenX * ActorScaleX);
			RightFloor->SetRelativeScale3D(FVector(FinalScaleX, FloorScale.Y, FloorScale.Z));

			// 기본 Plane 메시는 피벗이 중앙에 있으므로, 길이에 맞춰 중앙 X 위치를 스케일 역보정하여 배치합니다.
			float RightWorldCenterX = -HalfLength + GapEnd + (RightLen * 0.5f);
			RightFloor->SetRelativeLocation(FVector(RightWorldCenterX / ActorScaleX, FloorMesh->GetRelativeLocation().Y, FloorMesh->GetRelativeLocation().Z));
			GapFloorPieces.Add(RightFloor);
		}
	}

	bHasGap = true;
}

// ──────────────────────────────────────────────
// Gap 해제: 바닥 조각 파괴, FloorMesh 복원, 곡선 스플라인 복원
// ──────────────────────────────────────────────
void AExFloorChunk::ClearGap()
{
	if (!bHasGap) return;

	// 곡선 구간 동적 생성된 바닥 조각 파괴
	for (USplineMeshComponent* Piece : GapCurveSplinePieces)
	{
		if (Piece)
		{
			Piece->DestroyComponent();
		}
	}
	GapCurveSplinePieces.Empty();

	// 직선 구간 동적 생성된 편평 바닥 조각 제거
	for (UStaticMeshComponent* Piece : GapFloorPieces)
	{
		if (Piece)
		{
			Piece->DestroyComponent();
		}
	}
	GapFloorPieces.Empty();

	// 커브 스플라인 복원
	if (bHasCurve)
	{
		for (USplineMeshComponent* SplineMesh : CurveSplineMeshes)
		{
			if (SplineMesh)
			{
				SplineMesh->SetVisibility(true, true);
				SplineMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			}
		}
	}

	// 원본 FloorMesh 복원
	if (FloorMesh)
	{
		FloorMesh->SetVisibility(true, true);
		FloorMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	bHasGap = false;

	UE_LOG(LogExFloorChunk, Log, TEXT("[%s] ClearGap: FloorMesh restored"), *GetName());
}

// ──────────────────────────────────────────────
// 청크 활성화 - 회전 포함 (커브 지원)
// ──────────────────────────────────────────────
void AExFloorChunk::ActivateChunkWithRotation(const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	// 기본 활성화 수행
	ActivateChunk(SpawnLocation);

	// 회전 적용
	SetActorRotation(SpawnRotation);

	UE_LOG(LogExFloorChunk, Log, TEXT("[%s] ActivateChunkWithRotation: Loc=%s, Rot=%s"),
		*GetName(), *SpawnLocation.ToString(), *SpawnRotation.ToString());
}

// ──────────────────────────────────────────────
// 커브 적용: FloorMesh 숨기고 Spline Mesh로 원호 바닥 생성
// ──────────────────────────────────────────────
void AExFloorChunk::ApplyCurve(float Angle, float Radius, int32 SegmentCount, bool bIsLeftCurve, float HeightOffset)
{
	// 이미 커브 적용 중이면 먼저 해제
	if (bHasCurve) ClearCurve();
	if (!FloorMesh || !FloorMesh->GetStaticMesh()) return;

	// 직선인 경우 (Angle == 0) 처리
	if (FMath::IsNearlyZero(Angle))
	{
		SegmentCount = 1; // 직선은 1개 세그먼트면 충분
	}
	else if (SegmentCount < 1) 
	{
		SegmentCount = 1;
	}

	UStaticMesh* OrigMesh = FloorMesh->GetStaticMesh();

	// ── 1. 부모 스케일 역보정 (Inverse Scale) ──
	// SceneRoot(부모)에 스케일(예: 10, 4, 0.1)이 적용되어 있다면,
	// 자식 SplineMesh의 좌표/접선을 그대로 설정하면 이중으로 적용되어 터짐.
	// 따라서 부모 스케일로 나누어 "로컬 좌표"를 구해야 함.
	FVector ParentsScale = GetActorRelativeScale3D();
	if (FMath::IsNearlyZero(ParentsScale.X)) ParentsScale.X = 1.f;
	if (FMath::IsNearlyZero(ParentsScale.Y)) ParentsScale.Y = 1.f;
	if (FMath::IsNearlyZero(ParentsScale.Z)) ParentsScale.Z = 1.f;

	// ── 캐시 데이터 저장 (장애물 회전 동기화용) ──
	CachedCurveAngle = Angle;
	CachedCurveRadius = Radius;
	bCachedIsLeftCurve = bIsLeftCurve;
	CachedHeightOffset = HeightOffset;
	
	// SegmentType은 Spawner에서 설정하겠지만, 자체적으로도 정확하게 유추(보완)
	if (FMath::IsNearlyZero(Angle))
	{
		SegmentType = EExPathSegmentType::Straight;
	}
	else
	{
		SegmentType = bIsLeftCurve ? EExPathSegmentType::CurveLeft : EExPathSegmentType::CurveRight;
	}

	// ── 2. 원본 FloorMesh 숨기기 ──
	FloorMesh->SetVisibility(false, true);
	FloorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// ── 3. 원호(Arc) 데이터 계산 ──
	const float AngleRad = FMath::DegreesToRadians(Angle);
	const float SegAngleRad = AngleRad / SegmentCount;
	// RotSign: 좌커브(-1), 우커브(+1)
	const float RotSign = bIsLeftCurve ? -1.f : 1.f;

	// 원의 중심 (로컬 좌표, 청크 원점 기준)
	// 월드 반경을 로컬 스케일로 변환
	const float ScaledRadiusY = Radius / ParentsScale.Y; 
	// CurveRadius는 보통 X/Y 평면에서의 반경. 
	// 중심 좌표 Y = +/- Radius.
	const FVector LocalCenter = FVector(0.f, RotSign * ScaledRadiusY, 0.f);
	
	// 시작점에서 중심까지의 반지름 벡터 (로컬)
	const FVector RadialStart = FVector(0.f, 0.f, 0.f) - LocalCenter;

	// 높이 변화량 분배 (경사 구현) - Z 스케일 역보정
	// HeightOffset은 월드 높이. 로컬 Z로 변환.
	const float ScaledHeightOffset = HeightOffset / ParentsScale.Z;
	const float HeightPerSegment = ScaledHeightOffset / SegmentCount;

	// 경사 비율 (Tangent Z 보정용) - 접선은 스케일의 영향을 받으므로 신중해야 함.
	// 단순하게 Start/End Point Z 차이로 경사 형성.
	// Tangent Z는 0으로 두면 평평하게 시작해서 올라감.
	// 자연스러운 경사를 위해 Tangent Z도 기울기에 맞춤.
	// Slope = Height / ArcLength.
	// Tangent Z = TangentXY_Mag * Slope.
	const float TotalArcLength = Radius * AngleRad; // World Length
	// Tangent에 적용할 기울기는 (World Height / World Length) 가 적절.
	// 하지만 Tangent 벡터 자체도 ParentScale에 의해 스케일링됨.
	// LocalTangent Z = (WorldTangent Z) / ParentScale Z
	// WorldTangent Z = WorldTangent XY * SlopeRatio
	// 복잡하므로, Start/End Z 차이로 Spline이 알아서 처리하게 둠 (Tangent Z = 0보다는 나을 수 있음)
	// 여기서는 간단히 Tangent Z = HeightPerSeg (세그먼트 당 높이)로 근사.
	// 접선 크기가 세그먼트 길이와 비슷하므로.

	// 범위: [-Angle/2, +Angle/2]
	const float HalfTotalAngle = AngleRad * 0.5f;

	for (int32 i = 0; i < SegmentCount; ++i)
	{
		// Segment Start/End 계산
		FVector SegStart, SegEnd, StartTangent, EndTangent;

		if (FMath::IsNearlyZero(Angle))
		{
			// ── 직선 케이스 (Angle = 0) ──
			// 월드 길이 동기화 (기존 고정 1000.f -> ChunkLength)
			const float SafeChunkLen = ChunkLength;
			const float ScaledLenX = SafeChunkLen / ParentsScale.X;
			const float SegLen = ScaledLenX / SegmentCount;

			const float XStart = -(ScaledLenX * 0.5f) + (SegLen * i);
			const float XEnd = XStart + SegLen;

			SegStart = FVector(XStart, 0.f, HeightPerSegment * i);
			SegEnd = FVector(XEnd, 0.f, HeightPerSegment * (i + 1));
			
			StartTangent = FVector(SegLen, 0.f, HeightPerSegment);
			EndTangent = FVector(SegLen, 0.f, HeightPerSegment);
		}
		else
		{
			// ── 커브 케이스 ──
			const float StartAngle = -HalfTotalAngle + (SegAngleRad * i);
			const float EndAngle = -HalfTotalAngle + (SegAngleRad * (i + 1));

			// 원호 위의 시작/끝 지점 (로컬 - Center 기준)
			// RadialStart는 이미 Y스케일 보정됨.
			// 회전은 X,Y 평면에서 이루어짐.
			const FVector StartRadial = RadialStart.RotateAngleAxis(
				FMath::RadiansToDegrees(StartAngle) * RotSign, FVector::UpVector);
			const FVector EndRadial = RadialStart.RotateAngleAxis(
				FMath::RadiansToDegrees(EndAngle) * RotSign, FVector::UpVector);

			// Center + Radial
			SegStart = LocalCenter + StartRadial;
			SegEnd = LocalCenter + EndRadial;

			// X축 스케일 보정? 
			// 위 로직에서 LocalCenter(Y보정됨)와 Rotating(X,Y 혼합)을 사용함.
			// 만약 Parent X,Y 스케일이 다르다면 원이 타원이 될 수 있음.
			// 하지만 보통 Floor는 균일 스케일(Non-uniform scale on floor?)
			// Runner Game Floor Usually (10, 4, 1) -> Non-uniform.
			// X=10, Y=4.
			// Radius 1500.
			// LocalCenter.Y = 1500/4 = 375.
			// StartRadial(Rotated) has X and Y components.
			// X component needs to be divided by ParentScale.X?
			// Y component needs to be divided by ParentScale.Y?
			// 현재 StartRadial은 VectorLength 375짜리 벡터임.
			// 이걸 회전시키면 X,Y가 섞임.
			// 예: 90도 회전 -> X=375, Y=0.
			// World X = 375 * 10 = 3750.
			// World Radius was 1500. 3750 != 1500.
			// ★ 문제: Non-uniform scale에서 "회전"을 로컬에서 하면 찌그러짐.
			
			// ★ 해결: "World Space"에서 점을 계산하고, "Inverse Transform"으로 로컬로 변환해야 정확함.
			// 하지만 ApplyCurve는 Local Position을 설정해야 함.
			
			// 정확한 로직:
			// 1. World 기준의 Center, Start, End 계산
			// 2. ParentScale로 나누어 Local로 변환 (X는 /Scale.X, Y는 /Scale.Y)
			
			const float WorldRadius = Radius;
			const FVector WorldCenterLocal = FVector(0.f, RotSign * WorldRadius, 0.f); // 청크 중심 기준 월드 오프셋
			const FVector WorldRadialStart = -WorldCenterLocal;
			
			FVector WorldPosStart = WorldCenterLocal + WorldRadialStart.RotateAngleAxis(FMath::RadiansToDegrees(StartAngle) * RotSign, FVector::UpVector);
			FVector WorldPosEnd = WorldCenterLocal + WorldRadialStart.RotateAngleAxis(FMath::RadiansToDegrees(EndAngle) * RotSign, FVector::UpVector);
			
			// Scale 역보정
			SegStart = FVector(WorldPosStart.X / ParentsScale.X, WorldPosStart.Y / ParentsScale.Y, 0.f);
			SegEnd   = FVector(WorldPosEnd.X / ParentsScale.X, WorldPosEnd.Y / ParentsScale.Y, 0.f);

			// 높이(Z) 적용
			// ★ 중요: Actor는 세그먼트의 중간 지점(Center)에 위치함.
			//    따라서 로컬 Z=0은 세그먼트의 중간 높이여야 함.
			//    시작점은 -HalfHeight, 끝점은 +HalfHeight가 되어야 전체 높이 변화량(HeightOffset)을 만족하며 위치가 맞음.
			const float ZStartRel = (HeightPerSegment * i) - (ScaledHeightOffset * 0.5f);
			const float ZEndRel   = (HeightPerSegment * (i + 1)) - (ScaledHeightOffset * 0.5f);

			SegStart.Z = ZStartRel;
			SegEnd.Z   = ZEndRel;

			// 접선 (Tangent)
			// World Tangent (Spline Mesh Tangent is NOT a point, it's a vector)
			// Tangent Magnitude = Radius * AngleRad
			const float WorldTangentMag = WorldRadius * SegAngleRad;
			
			FVector WorldTangentStart = FVector::CrossProduct(FVector::UpVector, (WorldPosStart - WorldCenterLocal)).GetSafeNormal() * RotSign * WorldTangentMag;
			FVector WorldTangentEnd   = FVector::CrossProduct(FVector::UpVector, (WorldPosEnd - WorldCenterLocal)).GetSafeNormal() * RotSign * WorldTangentMag;

			// 접선 역보정 (Vector component division)
			StartTangent = FVector(WorldTangentStart.X / ParentsScale.X, WorldTangentStart.Y / ParentsScale.Y, WorldTangentStart.Z / ParentsScale.Z);
			EndTangent   = FVector(WorldTangentEnd.X / ParentsScale.X, WorldTangentEnd.Y / ParentsScale.Y, WorldTangentEnd.Z / ParentsScale.Z);
			
			// 접선 Z (경사)
			// World Tangent Z += Pitch related...
			// 여기선 간단히 0으로 두거나, Start/End Z 차이로 인해 Spline이 알아서 연결함.
			// 단, HeightPerSegment 만큼 Z를 주면 더 부드러움.
			StartTangent.Z = HeightPerSegment;
			EndTangent.Z = HeightPerSegment;
		}

		// SplineMeshComponent 생성
		FName SplineName = *FString::Printf(TEXT("CurveSpline_%d"), i);
		USplineMeshComponent* SplineMesh = NewObject<USplineMeshComponent>(this, 
			USplineMeshComponent::StaticClass(), SplineName);

		if (!SplineMesh) continue;

		SplineMesh->SetStaticMesh(OrigMesh);

		// 원본 머티리얼 복사
		for (int32 MatIdx = 0; MatIdx < FloorMesh->GetNumMaterials(); ++MatIdx)
		{
			SplineMesh->SetMaterial(MatIdx, FloorMesh->GetMaterial(MatIdx));
		}

		SplineMesh->SetForwardAxis(ESplineMeshAxis::X);

		// 시작/끝 위치 및 접선 설정
		SplineMesh->SetStartAndEnd(
			SegStart, StartTangent,
			SegEnd, EndTangent);

		// 뱅킹(Roll) 제거
		SplineMesh->SetStartRoll(0.f);
		SplineMesh->SetEndRoll(0.f);

		// ★ 스케일 설정: FloorMesh의 스케일을 그대로 반영해야 함
		// FloorMesh가 (10, 4, 0.1)이라면, SplineMesh의 단면은 (4, 0.1)이어야 함.
		// SplineMesh의 X축은 길이(Length)이므로, Scale X는 무시됨 (길이는 점 간 거리로 결정)
		// 따라서 Scale Y, Scale Z를 적용.
		FVector MeshScale = FloorMesh->GetRelativeScale3D();
		SplineMesh->SetStartScale(FVector2D(MeshScale.Y, MeshScale.Z));
		SplineMesh->SetEndScale(FVector2D(MeshScale.Y, MeshScale.Z));

		SplineMesh->SetCollisionProfileName(TEXT("BlockAll"));
		SplineMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		SplineMesh->SetMobility(EComponentMobility::Movable);

		SplineMesh->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
		SplineMesh->RegisterComponent();

		CurveSplineMeshes.Add(SplineMesh);
	}

	bHasCurve = true;

	UE_LOG(LogExFloorChunk, Log, TEXT("[%s] ApplyCurve(Quadrant): Angle=%.1f, Radius=%.1f, HeightOffset=%.1f"),
		*GetName(), Angle, Radius, HeightOffset);
}

// ──────────────────────────────────────────────
// 커브 해제: Spline Mesh 제거, FloorMesh 복원
// ──────────────────────────────────────────────
void AExFloorChunk::ClearCurve()
{
	if (!bHasCurve) return;

	// 동적 생성된 Spline Mesh 제거
	for (USplineMeshComponent* SplineMesh : CurveSplineMeshes)
	{
		if (SplineMesh)
		{
			SplineMesh->DestroyComponent();
		}
	}
	CurveSplineMeshes.Empty();

	// 원본 FloorMesh 복원
	if (FloorMesh)
	{
		FloorMesh->SetVisibility(true, true);
		FloorMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	// 회전 초기화
	SetActorRotation(FRotator::ZeroRotator);

	bHasCurve = false;

	UE_LOG(LogExFloorChunk, Log, TEXT("[%s] ClearCurve: FloorMesh restored"), *GetName());
}

FTransform AExFloorChunk::GetLocalTransformAtDistance(float LocalDistance) const
{
	// 1. 유효 거리 클램프
	LocalDistance = FMath::Clamp(LocalDistance, 0.f, ChunkLength);

	// 부모 스케일 (역보정용)
	FVector ParentsScale = GetActorScale3D();
	// 안정성을 위해 0.001 이하로 떨어지지 않게 제한
	ParentsScale.X = FMath::Max(ParentsScale.X, KINDA_SMALL_NUMBER);
	ParentsScale.Y = FMath::Max(ParentsScale.Y, KINDA_SMALL_NUMBER);
	ParentsScale.Z = FMath::Max(ParentsScale.Z, KINDA_SMALL_NUMBER);

	// 2. 직선 모델
	if (SegmentType == EExPathSegmentType::Straight || FMath::IsNearlyZero(CachedCurveAngle))
	{
		const float HalfLength = ChunkLength * 0.5f;
		// X 좌표 계산 및 역스케일링 적용
		const float X = (-HalfLength + LocalDistance) / ParentsScale.X;

		FVector Pos(X, 0.f, 0.f);

		// 높이 보간 (HeightOffset이 있다면)
		if (!FMath::IsNearlyZero(CachedHeightOffset))
		{
			float Alpha = LocalDistance / ChunkLength;
			// Z 오프셋도 역스케일링 적용
			Pos.Z = (CachedHeightOffset * Alpha) / ParentsScale.Z;
		}
		
		// 회전: Pitch 계산
		FRotator Rot = FRotator::ZeroRotator;
		if (!FMath::IsNearlyZero(CachedHeightOffset))
		{
			Rot.Pitch = FMath::RadiansToDegrees(FMath::Atan2(CachedHeightOffset, ChunkLength));
		}
		return FTransform(Rot, Pos, FVector::OneVector);
	}

	// 3. 커브 처리 (원호 수학)
	const float AngleRad = FMath::DegreesToRadians(CachedCurveAngle);
	const float Alpha = LocalDistance / ChunkLength;

	// ★ 수정됨: ApplyCurve의 커브 모델은 중심 각도를 0으로 기준하여
	// 시작 각도를 -HalfTotalAngle, 끝 각도를 +HalfTotalAngle로 배치합니다.
	const float HalfTotalAngle = AngleRad * 0.5f;
	const float CurrentAngle = -HalfTotalAngle + (AngleRad * Alpha); 

	const float DirSign = bCachedIsLeftCurve ? -1.f : 1.f;

	// 로컬 회전 중심 (월드 크기 기준 계산 후 역스케일링)
	const float WorldRadius = CachedCurveRadius;
	const FVector WorldCenterLocal = FVector(0.f, WorldRadius * DirSign, 0.f);
	const FVector WorldRadialStart = -WorldCenterLocal;

	// 보간된 각도만큼 회전 수행
	float DegAngle = FMath::RadiansToDegrees(CurrentAngle);
	FVector WorldPos = WorldCenterLocal + WorldRadialStart.RotateAngleAxis(DegAngle * DirSign, FVector::UpVector);

	// Scale 역보정 (로컬로 변환해야 ActorToWorld 변환 시 복구됨)
	FVector Pos = FVector(WorldPos.X / ParentsScale.X, WorldPos.Y / ParentsScale.Y, 0.f);

	// ★ 수정됨: 높이(Z) 보간 및 역보정
	if (!FMath::IsNearlyZero(CachedHeightOffset))
	{
		float ZOffset = (CachedHeightOffset * Alpha) - (CachedHeightOffset * 0.5f);
		Pos.Z = ZOffset / ParentsScale.Z;
	}

	// 방향(Tangent)을 구하여 회전값 도출
	// 회전은 월드 기준으로 계산된 텐전트를 가져와서 로컬 벡터로 역변환시켜 회전값 산출
	FVector WorldTangent = FVector::CrossProduct(FVector::UpVector, (WorldPos - WorldCenterLocal)).GetSafeNormal() * DirSign;
	FVector LocalTangent = FVector(WorldTangent.X / ParentsScale.X, WorldTangent.Y / ParentsScale.Y, WorldTangent.Z / ParentsScale.Z);
	
	if (!FMath::IsNearlyZero(CachedHeightOffset))
	{
		LocalTangent.Z = CachedHeightOffset / ParentsScale.Z;
	}
	
	FRotator Rot = LocalTangent.Rotation();

	return FTransform(Rot, Pos, FVector::OneVector);
}

void AExFloorChunk::CalcCurveSplinePoint(float LocalDistance, float SpanDistance, FVector& OutPos, FVector& OutTangent) const
{
	LocalDistance = FMath::Clamp(LocalDistance, 0.f, ChunkLength);

	FVector ParentsScale = GetActorScale3D();
	ParentsScale.X = FMath::Max(ParentsScale.X, KINDA_SMALL_NUMBER);
	ParentsScale.Y = FMath::Max(ParentsScale.Y, KINDA_SMALL_NUMBER);
	ParentsScale.Z = FMath::Max(ParentsScale.Z, KINDA_SMALL_NUMBER);

	const float AngleRad = FMath::DegreesToRadians(CachedCurveAngle);
	const float Alpha = LocalDistance / ChunkLength;

	const float HalfTotalAngle = AngleRad * 0.5f;
	const float CurrentAngle = -HalfTotalAngle + (AngleRad * Alpha); 

	const float DirSign = bCachedIsLeftCurve ? -1.f : 1.f;
	const float WorldRadius = CachedCurveRadius;
	
	const FVector WorldCenterLocal = FVector(0.f, WorldRadius * DirSign, 0.f);
	const FVector WorldRadialStart = -WorldCenterLocal;

	float DegAngle = FMath::RadiansToDegrees(CurrentAngle);
	FVector WorldPos = WorldCenterLocal + WorldRadialStart.RotateAngleAxis(DegAngle * DirSign, FVector::UpVector);

	OutPos = FVector(WorldPos.X / ParentsScale.X, WorldPos.Y / ParentsScale.Y, 0.f);

	if (!FMath::IsNearlyZero(CachedHeightOffset))
	{
		float ZOffset = (CachedHeightOffset * Alpha) - (CachedHeightOffset * 0.5f);
		OutPos.Z = ZOffset / ParentsScale.Z;
	}

	const float SpanAngleRad = AngleRad * (SpanDistance / ChunkLength);
	const float WorldTangentMag = WorldRadius * SpanAngleRad;

	FVector WorldTangent = FVector::CrossProduct(FVector::UpVector, (WorldPos - WorldCenterLocal)).GetSafeNormal() * DirSign * WorldTangentMag;
	OutTangent = FVector(WorldTangent.X / ParentsScale.X, WorldTangent.Y / ParentsScale.Y, WorldTangent.Z / ParentsScale.Z);
	
	if (!FMath::IsNearlyZero(CachedHeightOffset))
	{
		OutTangent.Z = (CachedHeightOffset * (SpanDistance / ChunkLength)) / ParentsScale.Z;
	}
}

USplineMeshComponent* AExFloorChunk::SpawnGapSplineMesh(float StartDist, float EndDist, UMaterialInterface* Material)
{
	float SpanDist = EndDist - StartDist;
	if (SpanDist <= 1.f) return nullptr;

	FVector StartPos, StartTangent;
	FVector EndPos, EndTangent;
	
	CalcCurveSplinePoint(StartDist, SpanDist, StartPos, StartTangent);
	CalcCurveSplinePoint(EndDist, SpanDist, EndPos, EndTangent);

	FName SplineName = MakeUniqueObjectName(this, USplineMeshComponent::StaticClass(), TEXT("GapCurveSpline"));
	USplineMeshComponent* SplineMesh = NewObject<USplineMeshComponent>(this, USplineMeshComponent::StaticClass(), SplineName);

	if (!SplineMesh) return nullptr;

	SplineMesh->SetStaticMesh(FloorMesh->GetStaticMesh());
	if (Material)
	{
		for (int32 MatIdx = 0; MatIdx < FloorMesh->GetNumMaterials(); ++MatIdx)
		{
			SplineMesh->SetMaterial(MatIdx, Material);
		}
	}

	SplineMesh->SetForwardAxis(ESplineMeshAxis::X);
	
	// 누락된 스케일 및 롤링 값 적용 (원본 부모 메쉬의 스케일 유지)
	FVector MeshScale = FloorMesh->GetRelativeScale3D();
	SplineMesh->SetStartScale(FVector2D(MeshScale.Y, MeshScale.Z));
	SplineMesh->SetEndScale(FVector2D(MeshScale.Y, MeshScale.Z));
	SplineMesh->SetStartRoll(0.f);
	SplineMesh->SetEndRoll(0.f);

	SplineMesh->SetStartAndEnd(StartPos, StartTangent, EndPos, EndTangent);
	
	SplineMesh->SetCollisionProfileName(TEXT("BlockAll"));
	SplineMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SplineMesh->SetMobility(EComponentMobility::Movable); // 어태치 에러 대응

	SplineMesh->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	SplineMesh->RegisterComponent();

	GapCurveSplinePieces.Add(SplineMesh);
	return SplineMesh;
}
