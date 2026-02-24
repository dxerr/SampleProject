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
					FBox Bounds = GetFloorBounds();
					FVector Center = Bounds.GetCenter();
					FVector Extent = Bounds.GetExtent();
					
					// 바운딩 박스를 그립니다.
					DrawDebugBox(World, Center, Extent, FColor::Green, false, -1.f, 0, 5.f);
					
					// 바운드 최상단 약간 위쪽에 스탯 정보를 문자로 출력합니다.
					FString ChunkDebugStr = FString::Printf(TEXT("Dist: %.0f"), PathDistance);
					DrawDebugString(World, Center + FVector(0.0f, 0.0f, Extent.Z + 50.f), ChunkDebugStr, nullptr, FColor::White, 0.0f, false);
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

		// ★ 스케일 업데이트: SceneRoot(1,1,1) 아래에 붙으므로
		//    부모(FloorMesh)의 스케일(10,4,0.1)을 직접 적용해야 함.
		//    X는 길이 비율 * FloorScale.X가 되어야 함.
		// ★ 스케일 업데이트: SceneRoot(1,1,1) 아래에 붙으므로
		//    부모(FloorMesh)의 스케일(10,4,0.1)을 직접 적용해야 함.
		//    X는 길이 비율 * FloorScale.X가 되어야 함.
		// FVector FloorScale = FloorMesh->GetRelativeScale3D(); // 상단 선언 사용
		float FinalScaleX = (LeftLen / OrigMeshLenX) * FloorScale.X;
		// Y, Z는 FloorMesh 그대로 적용
		LeftFloor->SetRelativeScale3D(FVector(FinalScaleX, FloorScale.Y, FloorScale.Z));

		// ★ 위치: SceneRoot(1,1,1) 기준이므로 월드 좌표 그대로(Local로 변환 불필요) 사용 가능?
		//    아니, SceneRoot가 Actor의 (0,0,0)에 있으므로
		//    LeftCenter X는 로컬 좌표계에서 그대로 사용. 스케일 나눌 필요 없음.
		float LeftCenterX = (-HalfLen) + (LeftLen * 0.5f);
		LeftFloor->SetRelativeLocation(FVector(LeftCenterX, 0.f, 0.f));

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

		// ★ 스케일: 위와 동일
		// ★ 스케일: 위와 동일
		// FVector FloorScale = FloorMesh->GetRelativeScale3D(); // 상단 선언 사용
		float FinalScaleX = (RightLen / OrigMeshLenX) * FloorScale.X;
		RightFloor->SetRelativeScale3D(FVector(FinalScaleX, FloorScale.Y, FloorScale.Z));

		// ★ 위치: 스케일 나눌 필요 없음
		float RightCenterX = GapEndX + (RightLen * 0.5f);
		RightFloor->SetRelativeLocation(FVector(RightCenterX, 0.f, 0.f));

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
// ──────────────────────────────────────────────
// 커브 적용: FloorMesh 숨기고 Spline Mesh로 원호 바닥 생성
// ──────────────────────────────────────────────
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
	// SegmentType은 Spawner에서 설정하겠지만, 여기서도 유추 가능
	SegmentType = bIsLeftCurve ? EExPathSegmentType::CurveLeft : EExPathSegmentType::CurveRight;

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
			// 월드 길이 1000 가정
			const float SafeChunkLen = 1000.f;
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
	// 1. Clamp Input
	LocalDistance = FMath::Clamp(LocalDistance, 0.f, ChunkLength);

	// 2. 직선 처리
	if (SegmentType == EExPathSegmentType::Straight || FMath::IsNearlyZero(CachedCurveAngle))
	{
		// 직선: X축 이동 (Local X)
		FVector Pos(LocalDistance, 0.f, 0.f);
		// 높이 보간 (HeightOffset이 있다면)
		if (!FMath::IsNearlyZero(CachedHeightOffset))
		{
			float Alpha = LocalDistance / ChunkLength;
			Pos.Z = CachedHeightOffset * Alpha;
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
	// ArcLength = Radius * AngleRad
	// 근데 ChunkLength가 곧 ArcLength임.
	const float AngleRad = FMath::DegreesToRadians(CachedCurveAngle);
	// 진행 비율
	const float Alpha = LocalDistance / ChunkLength;
	const float CurrentAngle = AngleRad * Alpha;

	// 좌/우 부호
	// 좌커브: -Yaw 회전, Y좌표는 - (왼쪽)
	// 우커브: +Yaw 회전, Y좌표는 + (오른쪽)
	const float DirSign = bCachedIsLeftCurve ? -1.f : 1.f;

	// 로컬 회전 (Yaw)
	// 좌커브: -Theta, 우커브: +Theta
	float YawDeg = CachedCurveAngle * Alpha * DirSign;
	float ThetaRad = FMath::DegreesToRadians(YawDeg); // 부호 포함

	// 로컬 위치 (X, Y)
	// 원점(0,0)에서 시작.
	// X = R * sin(|Theta|)
	// Y = R * (1 - cos(|Theta|)) * DirSign
	// (우커브면 Y>0, 좌커브면 Y<0)
	
	// ★ 중요: ParentsScale 역보정 필요 여부?
	// Obstacle은 ExFloorChunk의 Child로 붙음.
	// ExFloorChunk 자체가 Scale이 되어 있다면? (보통 1,1,1)
	// 하지만 SceneRoot가 Scale이 되어 있다면?
	// "Local Transform"은 SceneRoot 아래에서의 좌표여야 함.
	// 그래야 AttachToComponent(Root) 했을 때 맞음.
	// 하지만 ApplyCurve에서는 "MeshScale"을 역보정해서 "Actual Size"를 맞췄음.
	// Obstacle은 스케일링되지 않은 좌표계(Meter 단위)를 원함?
	// 아니면 부모 스케일을 따름?
	// 보통 Obstacle은 (1,1,1)로 붙음.
	// 따라서 여기서 "부모 스케일이 적용된 후의 좌표"를 주면 안되고,
	// "부모 스케일을 고려하지 않은 미터 단위 좌표"를 주면 -> 부모 스케일에 의해 왜곡됨.
	// ★ 결론: LocalTransform은 "부모 스케일이 1일 때의 좌표"를 리턴해야 함.
	// 그리고 부모 스케일이 있다면, 그에 맞춰 역보정된 좌표를 줘야 하나?
	// 복잡함... "Chunk의 Local Space"는 이미 Scaled Space임.
	// ExFloorChunk Root의 Scale이 (1,1,1)이라고 가정. (보통 그렇다)
	// 만약 (1,1,1)이라면 R * sin(theta) 그대로 쓰면 됨.

	float X = CachedCurveRadius * FMath::Sin(FMath::Abs(ThetaRad));
	float Y = CachedCurveRadius * (1.f - FMath::Cos(FMath::Abs(ThetaRad))) * DirSign;
	
	// 높이 보간
	float Z = 0.f;
	if (!FMath::IsNearlyZero(CachedHeightOffset))
	{
		Z = CachedHeightOffset * Alpha;
	}

	FVector Pos(X, Y, Z);

	// 회전 구성
	FRotator Rot(0.f, YawDeg, 0.f);
	// Pitch (Slope)
	if (!FMath::IsNearlyZero(CachedHeightOffset))
	{
		Rot.Pitch = FMath::RadiansToDegrees(FMath::Atan2(CachedHeightOffset, ChunkLength));
	}

	return FTransform(Rot, Pos, FVector::OneVector);
}
