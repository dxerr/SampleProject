// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class IVaultReportTask;

/**
 * per-asset 스텝 태스크 팩토리 + 동기 실행 헬퍼.
 *
 * 각 Report*.cpp에서 팩토리를 구현한다. 패널은 이 팩토리로 태스크를 생성하고,
 * 블루프린트/EUW용 동기 Export 함수는 RunTaskSynchronously로 같은 태스크를 끝까지 실행한다.
 */
namespace VaultAssetCheckToolReport
{
	TSharedPtr<IVaultReportTask> MakeTextureTask();
	TSharedPtr<IVaultReportTask> MakeSkeletalMeshTask();
	TSharedPtr<IVaultReportTask> MakeStaticMeshTask();
	TSharedPtr<IVaultReportTask> MakeUMGTask();
	TSharedPtr<IVaultReportTask> MakeAnimationTask();
	TSharedPtr<IVaultReportTask> MakeSoundTask();
	TSharedPtr<IVaultReportTask> MakeFontTask();
	TSharedPtr<IVaultReportTask> MakeNiagaraTask();  // UE5 전용(UE4에서는 nullptr 스텁)
	TSharedPtr<IVaultReportTask> MakeCascadeTask();  // UE4 전용(UE5에서는 nullptr 스텁)

	/** 태스크를 동기로 끝까지 실행(Prepare → 모든 ProcessStep → Finalize). 성공 여부 반환. */
	bool RunTaskSynchronously(const TSharedPtr<IVaultReportTask>& Task, const FString& OutputPath);
}
