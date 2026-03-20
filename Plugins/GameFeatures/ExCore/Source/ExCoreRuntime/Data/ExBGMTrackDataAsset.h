// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Sound/QuartzQuantizationUtilities.h"
#include "ExBGMTrackDataAsset.generated.h"

class USoundBase;
class UExMusicPhaseDataAsset;

/**
 * 게임 내 BGM 트랙의 재생 설정(곡, 속도, 박자)을 1:1로 묶어 관리하는 데이터 에셋입니다.
 */
UCLASS(BlueprintType)
class EXCORERUNTIME_API UExBGMTrackDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 재생할 BGM 메타사운드 또는 사운드 에셋 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BGM")
	TObjectPtr<USoundBase> BGMAsset;

	/** 곡의 템포 (Beats Per Minute) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BGM", meta = (ClampMin = "30.0", ClampMax = "300.0"))
	float BPM = 140.f;

	/** 한 마디(Bar)를 구성하는 박자 수 (예: 4/4 박자의 경우 4) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BGM|TimeSignature")
	int32 TimeSignatureNumBeats = 4;

	/** 기준 1박자의 음표 길이 (예: 4분 음표=QuarterNote) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BGM|TimeSignature")
	EQuartzTimeSignatureQuantization TimeSignatureBeatType = EQuartzTimeSignatureQuantization::QuarterNote;

	/** 이 곡에서 사용할 동적 악기 믹싱/볼륨 페이딩 (Phase) 설정 데이터 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BGM|Phase")
	TObjectPtr<UExMusicPhaseDataAsset> MusicPhaseData;
};
