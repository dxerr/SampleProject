// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ExCoreSpawnDataAsset.generated.h"

/**
 * UExCoreSpawnDataAsset
 * 게임 모드의 스폰 설정을 데이터 드리븐 방식으로 관리하는 데이터 에셋
 * PawnClasses와 VisualOverrides를 설정하여 캐릭터 스폰 및 비주얼 교체를 제어
 */
UCLASS(BlueprintType)
class EXCORERUNTIME_API UExCoreSpawnDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * 사용 가능한 Pawn 클래스 목록
	 * 예: SandboxCharacter_CMC, SandboxCharacter_Mover 등
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pawns")
	TArray<TSubclassOf<APawn>> PawnClasses;

	/**
	 * 현재 선택된 Pawn 클래스 인덱스
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pawns", meta = (ClampMin = "0"))
	int32 SelectedPawnIndex = 0;

	/**
	 * Visual Override 클래스 목록
	 * 예: BP_Twinblast, BP_Manny, BP_Quinn 등
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	TArray<TSubclassOf<AActor>> VisualOverrides;

	/**
	 * 현재 선택된 Visual Override 인덱스
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals", meta = (ClampMin = "0"))
	int32 SelectedVisualIndex = 0;

	/**
	 * 컨테이너 폰의 메시를 숨길지 여부
	 * Visual Override 적용 시 원본 메시를 숨기면 깔끔하게 보임
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	bool bHideContainerMesh = true;

	/**
	 * Visual Actor의 Animation 설정을 ContainerPawn에 복사할지 여부
	 * true: Visual의 SkeletalMesh와 AnimClass를 ContainerPawn에 적용
	 *       (ContainerPawn이 Visual의 외형과 애니메이션으로 동작)
	 * false: Visual Actor를 별도로 부착하여 사용
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	bool bCopyAnimationFromVisual = false;

	/**
	 * 선택된 Pawn 클래스 반환
	 * @return 유효한 Pawn 클래스 또는 nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "Spawn")
	TSubclassOf<APawn> GetSelectedPawnClass() const
	{
		if (PawnClasses.IsValidIndex(SelectedPawnIndex))
		{
			return PawnClasses[SelectedPawnIndex];
		}
		return nullptr;
	}

	/**
	 * 선택된 Visual Override 클래스 반환
	 * @return 유효한 Actor 클래스 또는 nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "Spawn")
	TSubclassOf<AActor> GetSelectedVisualOverride() const
	{
		if (VisualOverrides.IsValidIndex(SelectedVisualIndex))
		{
			return VisualOverrides[SelectedVisualIndex];
		}
		return nullptr;
	}
};
