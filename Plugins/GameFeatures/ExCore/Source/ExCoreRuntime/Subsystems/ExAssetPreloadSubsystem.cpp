// Copyright ExFrameWork. All Rights Reserved.
// 파일: ExAssetPreloadSubsystem.cpp
// 목적: ExCore 범용 비동기 에셋 사전 로딩(캐시 워밍) 서브시스템 구현
// 작성: Antigravity
// 생성일: 2026-05-19

#include "ExAssetPreloadSubsystem.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY(LogExPreload);

// ─────────────────────────────────────────────────────────────────────────────
// 초기화 / 종료
// ─────────────────────────────────────────────────────────────────────────────

void UExAssetPreloadSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogExPreload, Log, TEXT("[ExAssetPreloadSubsystem] 초기화 완료."));
}

void UExAssetPreloadSubsystem::Deinitialize()
{
	// Swap으로 안전하게 순회하며 전체 핸들 캔슬
	TMap<FName, TSharedPtr<FStreamableHandle>> HandlesToCancel;
	Swap(HandlesToCancel, ActiveHandles);

	for (auto It = HandlesToCancel.CreateIterator(); It; ++It)
	{
		const FName& Key = It.Key();
		const TSharedPtr<FStreamableHandle>& Handle = It.Value();
		if (Handle.IsValid() && Handle->IsActive())
		{
			Handle->CancelHandle();
			UE_LOG(LogExPreload, Log, TEXT("[ExAssetPreloadSubsystem] Deinitialize: '%s' 핸들 캔슬."), *Key.ToString());
		}
	}

	CompletedContexts.Empty();
	FailedContexts.Empty();

#if !UE_BUILD_SHIPPING
	DebugRequestTimestamps.Empty();
	DebugAssetCounts.Empty();
#endif

	Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
// Core API
// ─────────────────────────────────────────────────────────────────────────────

void UExAssetPreloadSubsystem::PreloadAssets(
	FName ContextKey,
	const TArray<TSoftObjectPtr<UObject>>& AssetsToLoad,
	FExPreloadOptions Options)
{
	if (!ensureMsgf(!ContextKey.IsNone(), TEXT("[ExPreload] ContextKey가 None입니다.")))
	{
		return;
	}

	if (!ensureMsgf(AssetsToLoad.Num() > 0, TEXT("[ExPreload] '%s': AssetsToLoad가 비어있습니다."), *ContextKey.ToString()))
	{
		return;
	}

	// 진행 중인 키 중복 요청 거부
	if (ActiveHandles.Contains(ContextKey))
	{
		UE_LOG(LogExPreload, Warning,
			TEXT("[ExAssetPreloadSubsystem] '%s' 이미 로딩 진행 중. 새 요청을 무시합니다."),
			*ContextKey.ToString());
		return;
	}

	// 완료/실패 상태 초기화 후 재로드 허용
	CompletedContexts.Remove(ContextKey);
	FailedContexts.Remove(ContextKey);

	// Soft Pointer → FSoftObjectPath (TSet으로 O(1) 중복 제거)
	TSet<FSoftObjectPath> TargetSet;
	TargetSet.Reserve(AssetsToLoad.Num());
	for (const TSoftObjectPtr<UObject>& Asset : AssetsToLoad)
	{
		if (!Asset.IsNull())
		{
			TargetSet.Add(Asset.ToSoftObjectPath());
		}
	}

	if (TargetSet.Num() == 0)
	{
		UE_LOG(LogExPreload, Warning,
			TEXT("[ExAssetPreloadSubsystem] '%s': 유효한 에셋 경로가 없습니다."),
			*ContextKey.ToString());
		return;
	}

	TArray<FSoftObjectPath> Targets = TargetSet.Array();

	UE_LOG(LogExPreload, Log,
		TEXT("[ExAssetPreloadSubsystem] '%s' 프리로드 시작 (에셋 수: %d, 우선순위: %d)"),
		*ContextKey.ToString(), Targets.Num(), Options.LoadPriority);

#if !UE_BUILD_SHIPPING
	DebugRequestTimestamps.Add(ContextKey, FPlatformTime::Seconds());
	DebugAssetCounts.Add(ContextKey, Targets.Num());
#endif

	// UAssetManager 싱글톤 StreamableManager 사용 (UE5 표준)
	// 별도 FStreamableManager 인스턴스 생성 금지: 엔진 글로벌 로딩 큐와 캐시가 분리되어 중복 로딩/캐시 미스 발생 위험
	TSharedPtr<FStreamableHandle> NewHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Targets,
		FStreamableDelegate::CreateUObject(this, &UExAssetPreloadSubsystem::HandleAsyncLoadComplete, ContextKey),
		Options.LoadPriority
	);

	if (NewHandle.IsValid())
	{
		ActiveHandles.Add(ContextKey, NewHandle);
	}
}

void UExAssetPreloadSubsystem::CancelPreload(FName ContextKey)
{
	if (TSharedPtr<FStreamableHandle>* HandlePtr = ActiveHandles.Find(ContextKey))
	{
		if (HandlePtr->IsValid() && (*HandlePtr)->IsActive())
		{
			(*HandlePtr)->CancelHandle();
			UE_LOG(LogExPreload, Log,
				TEXT("[ExAssetPreloadSubsystem] '%s' 프리로드 취소."), *ContextKey.ToString());
		}
		ActiveHandles.Remove(ContextKey);
	}

	CompletedContexts.Remove(ContextKey);
	FailedContexts.Remove(ContextKey);

#if !UE_BUILD_SHIPPING
	DebugRequestTimestamps.Remove(ContextKey);
	DebugAssetCounts.Remove(ContextKey);
#endif
}

void UExAssetPreloadSubsystem::CancelAllPreloads()
{
	TMap<FName, TSharedPtr<FStreamableHandle>> HandlesToCancel;
	Swap(HandlesToCancel, ActiveHandles);

	for (auto It = HandlesToCancel.CreateIterator(); It; ++It)
	{
		const FName& Key = It.Key();
		const TSharedPtr<FStreamableHandle>& Handle = It.Value();
		if (Handle.IsValid() && Handle->IsActive())
		{
			Handle->CancelHandle();
			UE_LOG(LogExPreload, Log,
				TEXT("[ExAssetPreloadSubsystem] CancelAll: '%s' 취소."), *Key.ToString());
		}
	}

	CompletedContexts.Empty();
	FailedContexts.Empty();

#if !UE_BUILD_SHIPPING
	DebugRequestTimestamps.Empty();
	DebugAssetCounts.Empty();
#endif
}

bool UExAssetPreloadSubsystem::IsPreloadComplete(FName ContextKey) const
{
	return CompletedContexts.Contains(ContextKey);
}

bool UExAssetPreloadSubsystem::IsPreloadFailed(FName ContextKey) const
{
	return FailedContexts.Contains(ContextKey);
}

void UExAssetPreloadSubsystem::ClearPreloadResult(FName ContextKey)
{
	CompletedContexts.Remove(ContextKey);
	FailedContexts.Remove(ContextKey);

#if !UE_BUILD_SHIPPING
	DebugRequestTimestamps.Remove(ContextKey);
	DebugAssetCounts.Remove(ContextKey);
#endif
}

float UExAssetPreloadSubsystem::GetPreloadProgress(FName ContextKey) const
{
	if (CompletedContexts.Contains(ContextKey) || FailedContexts.Contains(ContextKey))
	{
		return 1.0f;
	}

	if (const TSharedPtr<FStreamableHandle>* HandlePtr = ActiveHandles.Find(ContextKey))
	{
		if (HandlePtr->IsValid())
		{
			return (*HandlePtr)->GetProgress();
		}
	}

	// 등록된 적 없는 키 — 미등록(1.0f "완료됨"과 명확히 구분)
	return -1.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
// 내부 콜백
// ─────────────────────────────────────────────────────────────────────────────

void UExAssetPreloadSubsystem::HandleAsyncLoadComplete(FName ContextKey)
{
	check(IsInGameThread());

	// 이미 취소된 컨텍스트는 무시 (CancelPreload에서 ActiveHandles 제거 완료)
	if (!ActiveHandles.Contains(ContextKey))
	{
		return;
	}

	// 핸들 참조 확보 후 제거 (Fire-and-Forget: 소유 포기)
	TSharedPtr<FStreamableHandle> Handle;
	if (TSharedPtr<FStreamableHandle>* HandlePtr = ActiveHandles.Find(ContextKey))
	{
		Handle = *HandlePtr;
	}
	ActiveHandles.Remove(ContextKey);

	// 성공/실패 구분
	const bool bSuccess = Handle.IsValid() && Handle->HasLoadCompleted();

	if (bSuccess)
	{
		CompletedContexts.Add(ContextKey);

#if !UE_BUILD_SHIPPING
		if (const double* StartTime = DebugRequestTimestamps.Find(ContextKey))
		{
			const double ElapsedMs = (FPlatformTime::Seconds() - *StartTime) * 1000.0;
			const int32 AssetCount = DebugAssetCounts.FindRef(ContextKey);
			UE_LOG(LogExPreload, Log,
				TEXT("[ExAssetPreloadSubsystem] '%s' 프리로드 완료 (에셋 %d개, %.1fms)"),
				*ContextKey.ToString(), AssetCount, ElapsedMs);
		}
#endif

		OnPreloadFinished.Broadcast(ContextKey);
	}
	else
	{
		FailedContexts.Add(ContextKey);
		UE_LOG(LogExPreload, Warning,
			TEXT("[ExAssetPreloadSubsystem] '%s' 프리로드 실패. 에셋 경로를 확인하십시오."),
			*ContextKey.ToString());

		OnPreloadFailed.Broadcast(ContextKey);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Debug API
// ─────────────────────────────────────────────────────────────────────────────

void UExAssetPreloadSubsystem::DebugDumpPreloadStatus() const
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogExPreload, Display, TEXT("========== ExPreload Status Dump =========="));
	UE_LOG(LogExPreload, Display, TEXT("  Active: %d"), ActiveHandles.Num());

	for (auto It = ActiveHandles.CreateConstIterator(); It; ++It)
	{
		const FName& Key = It.Key();
		const TSharedPtr<FStreamableHandle>& Handle = It.Value();
		const float Progress = Handle.IsValid() ? Handle->GetProgress() : -1.0f;
		const int32 AssetCount = DebugAssetCounts.FindRef(Key);
		UE_LOG(LogExPreload, Display, TEXT("    [LOADING] '%s' - %d assets, %.1f%%"),
			*Key.ToString(), AssetCount, Progress * 100.0f);
	}

	UE_LOG(LogExPreload, Display, TEXT("  Completed: %d"), CompletedContexts.Num());
	for (const FName& Key : CompletedContexts)
	{
		UE_LOG(LogExPreload, Display, TEXT("    [DONE] '%s'"), *Key.ToString());
	}

	UE_LOG(LogExPreload, Display, TEXT("  Failed: %d"), FailedContexts.Num());
	for (const FName& Key : FailedContexts)
	{
		UE_LOG(LogExPreload, Display, TEXT("    [FAIL] '%s'"), *Key.ToString());
	}
	UE_LOG(LogExPreload, Display, TEXT("============================================"));

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan,
			FString::Printf(TEXT("[ExPreload] Active:%d Done:%d Fail:%d"),
				ActiveHandles.Num(), CompletedContexts.Num(), FailedContexts.Num()));
	}
#endif
}
