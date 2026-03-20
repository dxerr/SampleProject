// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "Quartz/AudioMixerClockHandle.h"
#include "Sound/QuartzQuantizationUtilities.h"
#include "GameplayTagContainer.h"
#include "ExMusicManagerSubsystem.generated.h"

class UAudioComponent;
class UQuartzSubsystem;
class UExMusicPhaseDataAsset;
struct FExMusicLayerConfig;

DECLARE_LOG_CATEGORY_EXTERN(LogExMusic, Log, All);

/**
 * UExMusicManagerSubsystem
 * Quartz Clock + MetaSound Source를 활용한 BGM 관리 서브시스템
 * 
 * 주요 기능:
 * - Quartz Clock 생성/관리 및 BPM 제어
 * - MetaSound Source 기반 BGM 재생 (PlayQuantized로 비트 정렬)
 * - 비트/마디 이벤트를 ExGameplayEventSubsystem으로 브로드캐스트
 * 
 * 사용법:
 *   UExMusicManagerSubsystem* MusicMgr = GetWorld()->GetSubsystem<UExMusicManagerSubsystem>();
 *   MusicMgr->StartBGM(BGMSoundAsset, 140.f);
 */
UCLASS()
class EXCORERUNTIME_API UExMusicManagerSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	// ========== BGM 재생 제어 ==========

	/**
	 * BGM 재생 시작
	 * Quartz Clock을 생성하고 MetaSound Source를 비트 정렬(PlayQuantized)하여 재생합니다.
	 * @param InBGMSound 재생할 MetaSound Source (또는 SoundWave/SoundCue)
	 * @param BPM 분당 비트 수 (기본 140)
	 * @param TimeSignatureNumerator 박자 분자 (기본 4, 4/4 박자)
	 * @param TimeSignatureDenominator 박자 분모 (기본 4, 4/4 박자)
	 */
	UFUNCTION(BlueprintCallable, Category = "Ex|Music")
	void StartBGM(USoundBase* InBGMSound, float BPM = 140.f, int32 TimeSignatureNumerator = 4, int32 TimeSignatureDenominator = 4);

	/**
	 * BGM 정지
	 * 오디오를 페이드아웃 후 정지하고, Quartz Clock을 정리합니다.
	 * @param FadeOutDuration 페이드아웃 시간 (초)
	 */
	UFUNCTION(BlueprintCallable, Category = "Ex|Music")
	void StopBGM(float FadeOutDuration = 1.0f);

	/**
	 * BGM 일시정지/해제
	 * @param bPause true면 일시정지, false면 재개
	 */
	UFUNCTION(BlueprintCallable, Category = "Ex|Music")
	void PauseBGM(bool bPause);

	/**
	 * 현재 BGM이 재생 중인지 확인
	 */
	UFUNCTION(BlueprintPure, Category = "Ex|Music")
	bool IsBGMPlaying() const;

	// ========== BPM 제어 ==========

	/**
	 * BPM 실시간 변경
	 * Quartz Clock의 BPM을 즉시 변경합니다.
	 * @param NewBPM 새 BPM 값
	 */
	UFUNCTION(BlueprintCallable, Category = "Ex|Music")
	void SetBPM(float NewBPM);

	/**
	 * 현재 BPM 반환
	 */
	UFUNCTION(BlueprintPure, Category = "Ex|Music")
	float GetCurrentBPM() const { return CurrentBPM; }

	// ========== 비트 이벤트 ==========

	/**
	 * 현재 비트 인덱스 반환 (0~TimeSignatureNumerator-1)
	 */
	UFUNCTION(BlueprintPure, Category = "Ex|Music")
	int32 GetCurrentBeatIndex() const { return CurrentBeatIndex; }

	/**
	 * 현재 마디 인덱스 반환
	 */
	UFUNCTION(BlueprintPure, Category = "Ex|Music")
	int32 GetCurrentBarIndex() const { return CurrentBarIndex; }

	/**
	 * Quartz Clock 핸들 직접 접근 (고급 사용자용)
	 */
	UFUNCTION(BlueprintPure, Category = "Ex|Music")
	UQuartzClockHandle* GetBGMClockHandle() const { return BGMClockHandle; }

	// ========== 2단계: Phase 레이어 믹싱 ==========

	/**
	 * Phase 데이터 에셋을 설정합니다.
	 * StartBGM 호출 전에 설정하면 자동으로 InitialPhase가 적용됩니다.
	 * @param InPhaseData Phase별 레이어 볼륨 프리셋 데이터 에셋
	 */
	UFUNCTION(BlueprintCallable, Category = "Ex|Music|Phase")
	void SetPhaseDataAsset(UExMusicPhaseDataAsset* InPhaseData);

	/**
	 * 음악 Phase 전환 (레이어 볼륨 보간 시작)
	 * Phase 데이터 에셋에 정의된 프리셋에 따라 레이어 볼륨이 부드럽게 전환됩니다.
	 * @param NewPhaseTag 전환할 Phase 태그 (예: ExMusicTags::Music_Phase_Climax)
	 */
	UFUNCTION(BlueprintCallable, Category = "Ex|Music|Phase")
	void TransitionToPhase(FGameplayTag NewPhaseTag);

	/**
	 * 개별 레이어 볼륨 직접 설정 (Phase 프리셋 무시)
	 * @param LayerName 레이어 식별자
	 * @param TargetVolume 목표 볼륨 (0~1)
	 * @param TransitionTime 전환 시간 (초)
	 */
	UFUNCTION(BlueprintCallable, Category = "Ex|Music|Phase")
	void SetLayerVolume(FName LayerName, float TargetVolume, float TransitionTime = 1.0f);

	/**
	 * 현재 음악 Phase 태그 반환
	 */
	UFUNCTION(BlueprintPure, Category = "Ex|Music|Phase")
	FGameplayTag GetCurrentMusicPhase() const { return CurrentMusicPhase; }

protected:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	/** Tick: 레이어 볼륨 보간 처리 */
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	// ========== Quartz 메트로놈 콜백 ==========

	/**
	 * Quartz 비트 이벤트 핸들러 (SubscribeToQuantizationEvent의 콜백)
	 * 매 비트(Quarter Note)마다 호출되어 GameplayEvent로 브로드캐스트
	 */
	UFUNCTION()
	void HandleBeatEvent(FName ClockName, EQuartzCommandQuantization QuantizationType, int32 NumBars, int32 Beat, float BeatFraction);

	/**
	 * Quartz 마디 이벤트 핸들러
	 * 매 마디(Bar)마다 호출
	 */
	UFUNCTION()
	void HandleBarEvent(FName ClockName, EQuartzCommandQuantization QuantizationType, int32 NumBars, int32 Beat, float BeatFraction);

	// ========== 내부 헬퍼 ==========

	/** Quartz Clock 생성 및 설정 */
	void CreateQuartzClock(float BPM, int32 Numerator, int32 Denominator);

	/** Quartz Clock 정리 */
	void DestroyQuartzClock();

	/** 메트로놈 이벤트 구독 */
	void SubscribeToMetronomeEvents();

	/** BGM 오디오 컴포넌트 생성 */
	UAudioComponent* CreateBGMAudioComponent(USoundBase* InSound);

	/** 레이어 런타임 상태 초기화 (Phase 데이터 에셋 기반) */
	void InitializeLayerStates();

	/** 레이어 볼륨 보간 업데이트 (Tick에서 호출) */
	void UpdateLayerVolumes(float DeltaTime);

	// ========== 멤버 변수 ==========

	/** Quartz Clock 핸들 */
	UPROPERTY()
	TObjectPtr<UQuartzClockHandle> BGMClockHandle;

	/** BGM 베이스 레이어 오디오 컴포넌트 */
	UPROPERTY()
	TObjectPtr<UAudioComponent> BaseLayerAudioComp;

	/** Phase 데이터 에셋 (레이어 구성 + Phase 프리셋 정의) */
	UPROPERTY()
	TObjectPtr<UExMusicPhaseDataAsset> PhaseDataAsset;

	/** 현재 BPM */
	float CurrentBPM = 140.f;

	/** 현재 비트 인덱스 (0~TimeSignatureNumerator-1) */
	int32 CurrentBeatIndex = 0;

	/** 현재 마디 인덱스 */
	int32 CurrentBarIndex = 0;

	/** 박자 정보 (분자) */
	int32 TimeSignatureNum = 4;

	/** BGM 재생 여부 */
	bool bIsPlaying = false;

	/** 현재 음악 Phase */
	FGameplayTag CurrentMusicPhase;

	// ── 레이어 런타임 상태 ──

	/** 레이어별 현재 볼륨 (보간 중 실시간 값) */
	TMap<FName, float> LayerCurrentVolumes;

	/** 레이어별 목표 볼륨 */
	TMap<FName, float> LayerTargetVolumes;

	/** 레이어별 보간 속도 (초당 변화량) */
	TMap<FName, float> LayerInterpSpeeds;

	/** Quartz Clock 이름 (고유) */
	static const FName BGMClockName;
};
