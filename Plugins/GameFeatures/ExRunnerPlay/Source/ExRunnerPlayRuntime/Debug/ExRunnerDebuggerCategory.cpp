// Copyright ExFrameWork. All Rights Reserved.

#include "ExRunnerDebuggerCategory.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "GameplayDebuggerCategory.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogExRunnerDebugger, Log, All);

// ========== 생성자 ==========

FExRunnerDebuggerCategory::FExRunnerDebuggerCategory()
{
	// 데이터팩 등록 (네트워크 복제가 필요한 경우 사용)
}

// ========== 카테고리 팩토리 ==========

TSharedRef<FGameplayDebuggerCategory> FExRunnerDebuggerCategory::MakeInstance()
{
	return MakeShareable(new FExRunnerDebuggerCategory());
}

// ========== 데이터 수집 ==========

void FExRunnerDebuggerCategory::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	if (!OwnerPC) return;

	// TODO: Phase 3 확장 시점에 실제 PathManager, ChunkSpawner 참조 연동
	// 현재는 플레이스홀더 데이터로 구조만 확인
	
	// 속도 데이터 수집 예시
	if (APawn* Pawn = OwnerPC->GetPawn())
	{
		SpeedData.CurrentSpeed = Pawn->GetVelocity().Size();
	}

	// PathManager 데이터 수집은 ExRunnerPlay의 컴포넌트에서 가져와야 함
	// Feature 내부 접근이므로 의존성 문제 없음
}

// ========== HUD 그리기 ==========

void FExRunnerDebuggerCategory::DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	// 헤더
	CanvasContext.Printf(TEXT("{yellow}===== ExRunner Debug ====="));

	// 경로 정보
	CanvasContext.Printf(TEXT("{white}[Path] Segments: {cyan}%d{white}  Distance: {cyan}%.1f{white}  Alpha: {cyan}%.3f"),
		PathData.SegmentCount, PathData.TotalDistance, PathData.CurrentAlpha);

	// 청크 정보
	CanvasContext.Printf(TEXT("{white}[Chunk] Active: {cyan}%d{white}  Pool: {cyan}%d{white}  Total: {cyan}%d"),
		ChunkData.ActiveChunks, ChunkData.PoolSize, ChunkData.TotalSpawned);

	// 경사 정보
	CanvasContext.Printf(TEXT("{white}[Slope] Active: %s  Height: {cyan}%.2f{white}  Pitch: {cyan}%.1f°{white}  Turns: {cyan}%d"),
		SlopeData.bSlopeActive ? TEXT("{green}ON") : TEXT("{red}OFF"),
		SlopeData.HeightOffset, SlopeData.SlopePitchAngle, SlopeData.ConsecutiveTurnCount);

	// 속도 정보
	CanvasContext.Printf(TEXT("{white}[Speed] Current: {cyan}%.0f{white}  Target: {cyan}%.0f"),
		SpeedData.CurrentSpeed, SpeedData.TargetSpeed);
}

#endif // WITH_GAMEPLAY_DEBUGGER
