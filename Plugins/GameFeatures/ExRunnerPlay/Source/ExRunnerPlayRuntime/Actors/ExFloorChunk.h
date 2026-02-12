// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Struct/FExPathSegment.h"
#include "ExFloorChunk.generated.h"

class AExRunnerGameMode;
class USplineMeshComponent;

/**
 * AExFloorChunk
 * 러너 게임의 이동하는 바닥 청크
 * 트레드밀 메커니즘: 플레이어는 고정, 바닥이 경로를 따라 이동
 * Spline Mesh를 통한 커브 바닥 렌더링 지원
 */
UCLASS()
class EXRUNNERPLAYRUNTIME_API AExFloorChunk : public AActor
{
	GENERATED_BODY()

public:
	AExFloorChunk();

	/**
	 * 기본 루트 컴포넌트 (스케일 독립성 보장용)
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/**
	 * 바닥 메시 컴포넌트
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> FloorMesh;

	/**
	 * 청크가 삭제될 X 좌표 (플레이어 뒤쪽)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Runner")
	float KillZ = -2000.f;

	/**
	 * 청크 길이 (X축, 다음 청크 스폰 위치 계산용)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Runner")
	float ChunkLength = 1000.f;

	/**
	 * 오브젝트 풀에서 관리 중인지 여부
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Runner")
	bool bIsPooled = false;

	/**
	 * 청크 활성화 - 직선 (풀에서 꺼낼 때)
	 */
	UFUNCTION(BlueprintCallable, Category = "Floor")
	void ActivateChunk(const FVector& SpawnLocation);

	/**
	 * 청크 활성화 - 회전 포함 (커브 지원)
	 * @param SpawnLocation 스폰 위치
	 * @param SpawnRotation 스폰 회전 (경로 접선 방향)
	 */
	UFUNCTION(BlueprintCallable, Category = "Floor")
	void ActivateChunkWithRotation(const FVector& SpawnLocation, const FRotator& SpawnRotation);

	/**
	 * 청크 비활성화 (풀로 반환)
	 */
	UFUNCTION(BlueprintCallable, Category = "Floor")
	void DeactivateChunk();

	/**
	 * 풀로 반환 (게임모드에 알림)
	 */
	UFUNCTION(BlueprintCallable, Category = "Floor")
	void ReturnToPool();

	// ── Gap 관련 ──

	/**
	 * Gap 적용: FloorMesh를 숨기고 양쪽에 바닥 조각 스폰
	 * @param GapLocalStartX  Gap 시작 위치 (청크 로컬 X, 왼쪽 끝 = -ChunkLength/2 기준)
	 * @param GapWidth        Gap 폭 (cm)
	 */
	UFUNCTION(BlueprintCallable, Category = "Floor|Gap")
	void ApplyGap(float GapLocalStartX, float GapWidth);

	/** Gap 해제: 바닥 조각 제거, FloorMesh 복원 */
	UFUNCTION(BlueprintCallable, Category = "Floor|Gap")
	void ClearGap();

	/** Gap 적용 중인지 여부 */
	UPROPERTY(BlueprintReadOnly, Category = "Floor|Gap")
	bool bHasGap = false;

	// ── 커브 관련 ──

	/**
	 * 커브 적용: FloorMesh를 숨기고 Spline Mesh로 원호 바닥 생성
	 * @param Angle   커브 총 각도 (도)
	 * @param Radius  커브 반경 (cm)
	 * @param SegmentCount  Spline Mesh 분할 수
	 * @param bIsLeftCurve  좌커브 여부
	 * @param HeightOffset  세그먼트 전체의 높이 변화량 (cm) - 꽈배기 방지용 경사
	 */
	UFUNCTION(BlueprintCallable, Category = "Floor|Curve")
	void ApplyCurve(float Angle, float Radius, int32 SegmentCount, bool bIsLeftCurve, float HeightOffset = 0.f);

	/** 커브 해제: Spline Mesh 제거, FloorMesh 복원 */
	UFUNCTION(BlueprintCallable, Category = "Floor|Curve")
	void ClearCurve();

	/** 커브 적용 중인지 여부 */
	UPROPERTY(BlueprintReadOnly, Category = "Floor|Curve")
	bool bHasCurve = false;

	/** 이 청크의 경로 상 누적 거리 */
	UPROPERTY(BlueprintReadOnly, Category = "Floor|Path")
	float PathDistance = 0.f;

	/** 이 청크의 세그먼트 타입 */
	UPROPERTY(BlueprintReadOnly, Category = "Floor|Path")
	EExPathSegmentType SegmentType = EExPathSegmentType::Straight;

	// ── 캐시된 커브 데이터 (GetLocalTransformAtDistance용) ──
	UPROPERTY(BlueprintReadOnly, Category = "Floor|Path")
	float CachedCurveAngle = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Floor|Path")
	float CachedCurveRadius = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Floor|Path")
	bool bCachedIsLeftCurve = false;

	UPROPERTY(BlueprintReadOnly, Category = "Floor|Path")
	float CachedHeightOffset = 0.f;

	/**
	 * 로컬 거리(Chunk Start 기준)에 해당하는 로컬 Transform 반환
	 * 장애물 스폰 시 PathManager 의존 없이 청크 자체적으로 위치 계산
	 */
	UFUNCTION(BlueprintPure, Category = "Floor|Path")
	FTransform GetLocalTransformAtDistance(float LocalDistance) const;

	UFUNCTION(BlueprintPure, Category = "Floor")
	FBox GetFloorBounds() const;

	/**
	 * 청크가 KillZ에 도달했을 때 호출되는 델리게이트
	 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChunkReachedKillZ, AExFloorChunk*, Chunk);
	UPROPERTY(BlueprintAssignable, Category = "Runner")
	FOnChunkReachedKillZ OnChunkReachedKillZ;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	/**
	 * 캐시된 게임모드 참조
	 */
	UPROPERTY()
	TObjectPtr<class AExRunnerGameMode> CachedGameMode;

	/** Gap 적용 시 생성된 양쪽 바닥 조각 */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> GapFloorPieces;

	/** 커브 적용 시 생성된 Spline Mesh 컴포넌트 배열 */
	UPROPERTY()
	TArray<TObjectPtr<USplineMeshComponent>> CurveSplineMeshes;
};
