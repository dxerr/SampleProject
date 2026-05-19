// Copyright ExFrameWork. All Rights Reserved.
// 파일: ExAssetPreloadSubsystem.h
// 목적: ExCore 범용 비동기 에셋 사전 로딩(캐시 워밍) 서브시스템
// 작성: Antigravity
// 생성일: 2026-05-19

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Struct/Subsystems/FExPreloadOptions.h"
#include "ExAssetPreloadSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogExPreload, Log, All);

/** 프리로드 완료 알림 델리게이트 (Key만 통지 — Fire-and-Forget 원칙) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPreloadFinished, FName, ContextKey);

/** 프리로드 실패 알림 델리게이트 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPreloadFailed, FName, ContextKey);

/**
 * UExAssetPreloadSubsystem
 *
 * ExCore에 속하는 범용 비동기 에셋 사전 로딩(캐시 워밍) 서브시스템.
 *
 * 핵심 원칙 (Fire-and-Forget):
 *   - 비동기 로드 완료 후 스트리밍 핸들을 즉시 릴리즈한다.
 *   - 에셋은 엔진/OS 캐시에만 잔존하며, 서브시스템은 에셋을 소유하지 않는다.
 *   - 완료 델리게이트로 Key만 통지하고, 에셋 수명 관리는 소비 모듈의 책임이다.
 *
 * 사용 예시:
 *   auto* Preloader = GetGameInstance()->GetSubsystem<UExAssetPreloadSubsystem>();
 *   Preloader->PreloadAssets(TEXT("RunnerIngame"), SoftAssetArray, FExPreloadOptions());
 *   // 완료 통지 후: UObject* Loaded = MySoftPtr.Get();  // 캐시 히트로 즉시 반환
 */
UCLASS()
class EXCORERUNTIME_API UExAssetPreloadSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ─────────────────────────────────────────────
	//  Core API
	// ─────────────────────────────────────────────

	/**
	 * [범용 API] 특정 컨텍스트 키에 대해 지정된 소프트 에셋 목록을 백그라운드 비동기로 로드한다.
	 * 이미 완료된 키에 대한 재요청은 허용된다 (기존 상태를 초기화 후 새로 로드).
	 * 진행 중인 키에 대한 중복 요청은 거부된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ExPreload")
	void PreloadAssets(FName ContextKey, const TArray<TSoftObjectPtr<UObject>>& AssetsToLoad, FExPreloadOptions Options);

	/**
	 * [취소 및 해제 API] 진행 중인 비동기 로딩을 강제 캔슬한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ExPreload")
	void CancelPreload(FName ContextKey);

	/**
	 * [전체 취소 API] 모든 활성 프리로드를 일괄 캔슬한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ExPreload")
	void CancelAllPreloads();

	/**
	 * [체크 API] 특정 컨텍스트의 프리로드가 완료되었는지 여부를 반환한다.
	 */
	UFUNCTION(BlueprintPure, Category = "ExPreload")
	bool IsPreloadComplete(FName ContextKey) const;

	/**
	 * [진행률 API] 특정 컨텍스트의 로딩 진행률을 반환한다.
	 * - 완료/실패: 1.0f
	 * - 진행 중: 핸들의 실제 진행률 (0.0 ~ 1.0)
	 * - 미등록 키: -1.0f (완료와 구분)
	 */
	UFUNCTION(BlueprintPure, Category = "ExPreload")
	float GetPreloadProgress(FName ContextKey) const;

	/**
	 * [실패 여부 API] 특정 컨텍스트의 프리로드가 실패했는지 여부를 반환한다.
	 */
	UFUNCTION(BlueprintPure, Category = "ExPreload")
	bool IsPreloadFailed(FName ContextKey) const;

	/**
	 * [정리 API] 완료/실패 상태를 명시적으로 제거한다.
	 * 소비 모듈은 완료 통지 처리 후 호출하여 CompletedContexts/FailedContexts 누적을 방지해야 한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ExPreload")
	void ClearPreloadResult(FName ContextKey);

	// ─────────────────────────────────────────────
	//  Debug API
	// ─────────────────────────────────────────────

	/**
	 * [디버그] 현재 모든 프리로드 상태를 화면 및 로그에 덤프한다.
	 * Shipping 빌드에서는 동작하지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ExPreload|Debug")
	void DebugDumpPreloadStatus() const;

public:
	/** 비동기 프리로드 완료 알림 델리게이트 (Key만 통지) */
	UPROPERTY(BlueprintAssignable, Category = "ExPreload")
	FOnPreloadFinished OnPreloadFinished;

	/** 비동기 프리로드 실패 알림 델리게이트 */
	UPROPERTY(BlueprintAssignable, Category = "ExPreload")
	FOnPreloadFailed OnPreloadFailed;

private:
	/** 비동기 로드 완료/실패 콜백 */
	void HandleAsyncLoadComplete(FName ContextKey);

private:
	/** 컨텍스트별 현재 실행 중인 비동기 스트리밍 핸들 맵 */
	TMap<FName, TSharedPtr<FStreamableHandle>> ActiveHandles;

	/** 완료된 프리로드 상태 관리용 셋 (소비 모듈이 ClearPreloadResult로 명시 정리 필요) */
	TSet<FName> CompletedContexts;

	/** 실패한 프리로드 상태 관리용 셋 (소비 모듈이 ClearPreloadResult로 명시 정리 필요) */
	TSet<FName> FailedContexts;

#if !UE_BUILD_SHIPPING
	/** [디버그 전용] 컨텍스트별 요청 시각 (로딩 소요시간 측정용) */
	TMap<FName, double> DebugRequestTimestamps;

	/** [디버그 전용] 컨텍스트별 요청 에셋 수 */
	TMap<FName, int32> DebugAssetCounts;
#endif
};
