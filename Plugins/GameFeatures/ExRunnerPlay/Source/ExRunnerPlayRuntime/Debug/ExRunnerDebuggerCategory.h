// Copyright ExFrameWork. All Rights Reserved.
// ExRunnerPlay Gameplay Debugger 카테고리 — 실시간 Runner 데이터 오버레이

#pragma once

#include "CoreMinimal.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebuggerCategory.h"
#endif

/**
 * FExRunnerDebuggerCategory
 * Gameplay Debugger (') 키로 접근 가능한 ExRunner 전용 오버레이
 * 
 * 표시 정보:
 * - PathManager: 현재 세그먼트 수, 총 거리
 * - ChunkSpawner: 활성 청크 수, 풀 상태
 * - Slope: HeightOffset, SlopePitchAngle, ConsecutiveTurnCount
 * - Speed: 현재 속도, 목표 속도
 * 
 * 등록:
 *   모듈 StartupModule()에서 GameplayDebugger에 카테고리 등록
 */
#if WITH_GAMEPLAY_DEBUGGER

class EXRUNNERPLAYRUNTIME_API FExRunnerDebuggerCategory : public FGameplayDebuggerCategory
{
public:
	FExRunnerDebuggerCategory();

	/** 매 틱 데이터 수집*/
	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;

	/** HUD에 데이터 그리기 */
	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;

	/** 카테고리 팩토리 */
	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

private:
	// ===== 수집된 데이터 =====
	
	/** PathManager 정보 */
	struct FPathData
	{
		int32 SegmentCount = 0;
		float TotalDistance = 0.f;
		float CurrentAlpha = 0.f;
	} PathData;

	/** ChunkSpawner 정보 */
	struct FChunkData
	{
		int32 ActiveChunks = 0;
		int32 PoolSize = 0;
		int32 TotalSpawned = 0;
	} ChunkData;

	/** Slope 정보 */
	struct FSlopeData
	{
		float HeightOffset = 0.f;
		float SlopePitchAngle = 0.f;
		int32 ConsecutiveTurnCount = 0;
		bool bSlopeActive = false;
	} SlopeData;

	/** Speed 정보 */
	struct FSpeedData
	{
		float CurrentSpeed = 0.f;
		float TargetSpeed = 0.f;
	} SpeedData;
};

#endif // WITH_GAMEPLAY_DEBUGGER
