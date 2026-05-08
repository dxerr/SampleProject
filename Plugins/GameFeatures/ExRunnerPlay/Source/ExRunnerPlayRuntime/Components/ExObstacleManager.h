// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../Struct/FExPathSegment.h"
#include "../Struct/FExObstacleContext.h"
#include "FExSpawnPlan.h"
#include "Math/RandomStream.h"
#include "ExObstacleManager.generated.h"

USTRUCT()
struct FExObstacleSyncInfo
{
	GENERATED_BODY()

	UPROPERTY()
	AActor* Obstacle = nullptr;

	UPROPERTY()
	int32 SegmentIndex = -1;

	// === 클라이언트 시각 동기화 데이터 ===
	UPROPERTY()
	FVector ActorScale = FVector::OneVector;

	UPROPERTY()
	uint8 InfoType = 0; // EExObstacleType

	UPROPERTY()
	float InfoValue = 0.f; // GapWidth, TargetDepth 등 M 표시 수치

	UPROPERTY()
	float GapLocalStartDist = -1.f; // Gap 적용 지점 (-1이면 아님)

	UPROPERTY()
	float MeshRelativeScaleY = -1.f; // Mesh의 Y 스케일 보정값

	/**
	 * 서버가 CalculateSpawnPosition에서 계산한 최종 월드 위치.
	 * 클라이언트에서 AttachToActor 후 강제 적용하여 커브 구간 위치 오차를 보정한다.
	 */
	UPROPERTY()
	FVector WorldLocation = FVector::ZeroVector;

	/**
	 * 서버가 CalculateSpawnPosition에서 계산한 최종 월드 회전.
	 * 클라이언트에서 AttachToActor 후 강제 적용하여 커브 구간 회전 오차를 보정한다.
	 */
	UPROPERTY()
	FRotator WorldRotation = FRotator::ZeroRotator;

	bool operator==(const FExObstacleSyncInfo& Other) const
	{
		return Obstacle == Other.Obstacle;
	}
};


class AExFloorChunk;
class UExChunkSpawner;
class UExObstacleDefinition;
class UExObstacleSpawnStrategy;
class UExPathManager;
enum class EExObstacleType : uint8;

/**
 * UExObstacleManager
 * 청크 스포너의 이벤트(OnChunkSpawned)를 수신하여
 * 바닥 위에 장애물을 배치하고 관리하는 컴포넌트.
 */
UCLASS(ClassGroup=(ExCore), meta=(BlueprintSpawnableComponent))
class EXRUNNERPLAYRUNTIME_API UExObstacleManager : public UActorComponent
{
	GENERATED_BODY()

public:
public:
	UExObstacleManager();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * DataCenter에서 불러온 장애물 정의 목록 캐시
	 */
	UPROPERTY(Transient)
	TArray<UExObstacleDefinition*> CachedObstacleDefinitions;

	/**
	 * 타입별 스폰 전략 매핑 (Strategy Pattern)
	 * 에디터에서 각 EExObstacleType에 대응하는 전략을 인라인 편집 가능
	 */
	UPROPERTY(EditAnywhere, Instanced, Category = "Obstacle|Strategy")
	TMap<EExObstacleType, TObjectPtr<UExObstacleSpawnStrategy>> SpawnStrategies;

	/**
	 * 연결된 청크 스포너 
	 * (장애물 스폰 확률, 쿨타임 등의 통합 설정 참조용)
	 */
	UPROPERTY(Transient)
	UExChunkSpawner* BoundSpawner;

	/**
	 * 청크 스포너와 연결 (델리게이트 구독)
	 */
	UFUNCTION(BlueprintCallable, Category = "Obstacle")
	void BindToSpawner(UExChunkSpawner* Spawner);

	/**
	 * 비트 기반 장애물 스폰 요청 (BeatSyncComponent에서 호출)
	 */
	UFUNCTION(BlueprintCallable, Category = "Obstacle")
	void RequestBeatSpawn();

	/** 기존 청크 스폰 시 발생되는 장애물 배치를 무시할지 여부 (비트 동기화 시 true) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Obstacle")
	bool bSuppressDefaultChunkSpawn = false;

protected:
	virtual void BeginPlay() override;

	/**
	 * 청크가 생성되었을 때 호출 (장애물 배치)
	 */
	UFUNCTION()
	void OnChunkSpawned(AExFloorChunk* Chunk);

	/**
	 * 청크가 제거되기 직전에 호출 (장애물 회수)
	 */
	UFUNCTION()
	void OnChunkDespawned(AExFloorChunk* Chunk);

	/**
	 * 월드 시프트 이벤트 핸들러 (좌표 보정)
	 */

protected:
	// 장애물 풀: 클래스별로 스택 관리
	// UPROPERTY가 없어도 World에 Spawn된 Actor는 GC되지 않음 (Level이 참조)
	TMap<UClass*, TArray<AActor*>> ObstaclePool;



	// 내부 함수들
	AActor* GetObstacleFromPool(UClass* ObstacleClass);
	void ReturnObstacleToPool(AActor* Obstacle);

	void ActivateObstacle(AActor* Obstacle);
	void DeactivateObstacle(AActor* Obstacle);

public:
	void SpawnObstaclesOnChunk(AExFloorChunk* Chunk, float ChunkStartLocalX, float ChunkLength, bool bForceSpawn = false);

	/**
	 * 특정 PathDistance 근처에 장애물이 있는지 질의한다.
	 * UExRunnerItemManager가 아이템 Z축 배치를 결정할 때 호출.
	 * @param PathDist 질의할 경로 기반 거리
	 * @param QueryRadius 검색 반경 (cm)
	 * @param OutContext 결과가 채워지는 구조체
	 * @return 장애물이 발견되었으면 true
	 */
	UFUNCTION(BlueprintCallable, Category = "Obstacle")
	bool QueryObstacleAtDistance(float PathDist, float QueryRadius, FExObstacleContext& OutContext) const;

protected:
	bool CheckFeasibility(float CurrentSpawnX, float& OutNextSafeX);
	UExObstacleDefinition* SelectRandomDefinition() const;

	// Helper
	static FBoxSphereBounds GetVisualBounds(AActor* Actor);

	// ── 커브 구간 특수 배치 ──

	/**
	 * 커브 구간에서의 장애물 배치 제한 체크
	 * @param Chunk 배치 대상 청크
	 * @return 장애물 배치 허용 여부
	 */


	/** 경로 거리 기반 안전 배치 거리 */
	float LastObstacleSafeEndDistance = -99999.f;

	/** SharedTrackSeed 기반 파생 스트림 (Hash(Seed, 1)). */
	FRandomStream ObstacleRandomStream;

	/** ObstacleRandomStream 초기화 완료 여부. Generate 호출 전 반드시 true여야 함. */
	bool bRandomStreamInitialized = false;

public:
	// ── Phase 1: 결정론 RandomStream ──

	/**
	 * §3.1.1 초기화 계약 — SharedTrackSeed 수신 직후 호출.
	 * Hash(SharedSeed, 1)로 ObstacleRandomStream을 초기화한다.
	 * @param SharedSeed AExRunnerGameState::SharedTrackSeed 값
	 */
	void InitializeRandomStream(int32 SharedSeed);

	// ── Phase 2: Plan 기반 2단계 스폰 ──

	/**
	 * 결정론적 배치 산출 (양측 호출 가능, Authority 가드 없음).
	 * ObstacleRandomStream으로 장애물 레이아웃을 계산하고 FExSpawnPlan 배열을 반환한다.
	 */
	TArray<FExSpawnPlan> GenerateObstaclePlan(AExFloorChunk* Chunk);

	/**
	 * 서버 전용 실체화 — GenerateObstaclePlan의 결과로 실제 액터를 배치.
	 * 반드시 HasAuthority() == true인 컨텍스트에서만 호출해야 한다.
	 */
	void RealizeObstaclePlan(const TArray<FExSpawnPlan>& Plan, AExFloorChunk* Chunk);

	/**
	 * Plan 기반 장애물 질의 — GenerateItemPlan 내부에서 전용으로 사용 (§3.4 규칙).
	 * 실액터 조회 대신 Plan 배열을 직접 순회하므로 클라이언트에서도 정확히 동작한다.
	 */
	bool QueryObstaclePlanAtDistance(const TArray<FExSpawnPlan>& Plan, float PathDist, float QueryRadius, FExObstacleContext& OutContext) const;

	// ── Phase 4: 세그먼트 인덱스 기반 액터 추적 ──

	/** SegmentIndex별 스폰된 장애물 액터 인덱스. 청크 Despawn 시 일괄 회수. */
	TMap<int32, TArray<TWeakObjectPtr<AActor>>> SpawnedBySegment;

	/** 로컬 청크 스폰 시 호출하여 대기 중인 리플리케이트 장애물들의 Attach 처리를 시도한다. */
	void OnLocalChunkSpawned(AExFloorChunk* SpawnedChunk);

	// ── Phase 4: Replicated Array 방식 동기화 ──
	
	/** 서버에서 생성된 장애물과 SegmentIndex 매핑 정보 리플리케이션 */
	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedObstacles)
	TArray<FExObstacleSyncInfo> ReplicatedObstacles;

	/** 클라이언트 측에서 이미 처리(부착 및 동기화)를 완료한 장애물을 추적하기 위한 Set */
	UPROPERTY(Transient)
	TSet<AActor*> ClientAttachedObstacles;

	UFUNCTION()
	void OnRep_ReplicatedObstacles();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

