// Copyright ExFrameWork. All Rights Reserved.

#include "Subsystems/ExMusicManagerSubsystem.h"
#include "Events/ExGameplayEventSubsystem.h"
#include "Tags/ExMusicTags.h"
#include "Data/ExMusicPhaseDataAsset.h"
#include "Data/ExBGMTrackDataAsset.h"
#include "Quartz/QuartzSubsystem.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY(LogExMusic);

// Quartz Clock 고유 이름
const FName UExMusicManagerSubsystem::BGMClockName = TEXT("ExBGMClock");

// ────────────────────────────────────────────────────────────
// Subsystem 라이프사이클
// ────────────────────────────────────────────────────────────

bool UExMusicManagerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// 게임 월드에서만 생성 (에디터 프리뷰 제외)
	return true;
}

void UExMusicManagerSubsystem::Deinitialize()
{
	// 재생 중이면 정리
	if (bIsPlaying)
	{
		StopBGM(0.f);
	}

	Super::Deinitialize();
}

// ────────────────────────────────────────────────────────────
// BGM 재생 제어
// ────────────────────────────────────────────────────────────

void UExMusicManagerSubsystem::StartBGM(const UExBGMTrackDataAsset* TrackData)
{
	if (!ensure(TrackData && TrackData->BGMAsset))
	{
		UE_LOG(LogExMusic, Error, TEXT("[ExMusicManager] StartBGM 실패: BGM 트랙 데이터 에셋 또는 BGM 오디오 에셋이 유효하지 않습니다."));
		return;
	}

	// 이미 재생 중이면 먼저 정지
	if (bIsPlaying)
	{
		UE_LOG(LogExMusic, Warning, TEXT("[ExMusicManager] BGM이 이미 재생 중입니다. 기존 BGM을 정지 후 새로운 BGM을 시작합니다."));
		StopBGM(0.f);
	}

	CurrentBPM = TrackData->BPM;
	TimeSignatureNum = TrackData->TimeSignatureNumBeats;
	CurrentBeatIndex = 0;
	CurrentBarIndex = 0;

	// 1. Quartz Clock 생성
	CreateQuartzClock(TrackData->BPM, TrackData->TimeSignatureNumBeats, static_cast<int32>(TrackData->TimeSignatureBeatType));

	if (!ensure(BGMClockHandle))
	{
		UE_LOG(LogExMusic, Error, TEXT("[ExMusicManager] Quartz Clock 생성에 실패했습니다."));
		return;
	}

	// 2. 메트로놈 이벤트 구독 (비트/마디)
	SubscribeToMetronomeEvents();

	// 3. 오디오 컴포넌트 생성 및 PlayQuantized
	BaseLayerAudioComp = CreateBGMAudioComponent(TrackData->BGMAsset);

	if (!ensure(BaseLayerAudioComp))
	{
		UE_LOG(LogExMusic, Error, TEXT("[ExMusicManager] 오디오 컴포넌트 생성에 실패했습니다."));
		DestroyQuartzClock();
		return;
	}

	// PlayQuantized: 다음 비트 경계에서 재생 시작
	FQuartzQuantizationBoundary QuantBoundary;
	QuantBoundary.Quantization = EQuartzCommandQuantization::Beat;
	QuantBoundary.Multiplier = 1.f;
	QuantBoundary.CountingReferencePoint = EQuarztQuantizationReference::BarRelative;

	FOnQuartzCommandEventBP PlayDelegate;
	UQuartzClockHandle* ClockHandleRaw = BGMClockHandle;
	BaseLayerAudioComp->PlayQuantized(this, ClockHandleRaw, QuantBoundary, PlayDelegate);

	bIsPlaying = true;

	// 4. 이 곡 만을 위한 전용 Phase 데이터로 교체 및 초기화
	PhaseDataAsset = TrackData->MusicPhaseData;
	InitializeLayerStates();

	// 5. 초기 Phase 적용 (Phase 데이터 에셋이 설정된 경우)
	if (PhaseDataAsset && PhaseDataAsset->InitialPhaseTag.IsValid())
	{
		TransitionToPhase(PhaseDataAsset->InitialPhaseTag);
	}

	UE_LOG(LogExMusic, Log, TEXT("[ExMusicManager] BGM 시작: %s (BPM: %.1f, 박자: %d/%d)"),
		*TrackData->BGMAsset->GetName(), TrackData->BPM, TrackData->TimeSignatureNumBeats, static_cast<int32>(TrackData->TimeSignatureBeatType));
}

void UExMusicManagerSubsystem::StopBGM(float FadeOutDuration)
{
	if (!bIsPlaying)
	{
		return;
	}

	// 오디오 페이드아웃 후 정지
	if (BaseLayerAudioComp && BaseLayerAudioComp->IsPlaying())
	{
		if (FadeOutDuration > 0.f)
		{
			BaseLayerAudioComp->FadeOut(FadeOutDuration, 0.f);
		}
		else
		{
			BaseLayerAudioComp->Stop();
		}
	}

	// Quartz Clock 정리
	DestroyQuartzClock();

	// 오디오 컴포넌트 정리
	if (BaseLayerAudioComp)
	{
		BaseLayerAudioComp->DestroyComponent();
		BaseLayerAudioComp = nullptr;
	}

	bIsPlaying = false;
	CurrentBeatIndex = 0;
	CurrentBarIndex = 0;
	CurrentMusicPhase = FGameplayTag();
	LayerCurrentVolumes.Empty();
	LayerTargetVolumes.Empty();
	LayerInterpSpeeds.Empty();

	UE_LOG(LogExMusic, Log, TEXT("[ExMusicManager] BGM 정지 (FadeOut: %.1fs)"), FadeOutDuration);
}

void UExMusicManagerSubsystem::PauseBGM(bool bPause)
{
	if (!bIsPlaying || !BGMClockHandle)
	{
		return;
	}

	if (bPause)
	{
		UQuartzClockHandle* Handle = BGMClockHandle;
		BGMClockHandle->PauseClock(this, Handle);

		if (BaseLayerAudioComp)
		{
			BaseLayerAudioComp->SetPaused(true);
		}
	}
	else
	{
		UQuartzClockHandle* Handle = BGMClockHandle;
		BGMClockHandle->ResumeClock(this, Handle);

		if (BaseLayerAudioComp)
		{
			BaseLayerAudioComp->SetPaused(false);
		}
	}

	UE_LOG(LogExMusic, Log, TEXT("[ExMusicManager] BGM %s"), bPause ? TEXT("일시정지") : TEXT("재개"));
}

bool UExMusicManagerSubsystem::IsBGMPlaying() const
{
	return bIsPlaying && BaseLayerAudioComp && BaseLayerAudioComp->IsPlaying();
}

// ────────────────────────────────────────────────────────────
// BPM 제어
// ────────────────────────────────────────────────────────────

void UExMusicManagerSubsystem::SetBPM(float NewBPM)
{
	if (!BGMClockHandle || !bIsPlaying)
	{
		UE_LOG(LogExMusic, Warning, TEXT("[ExMusicManager] SetBPM 실패: BGM이 재생 중이 아닙니다."));
		return;
	}

	CurrentBPM = NewBPM;

	// 비트 경계에서 BPM 변경 (부드러운 전환)
	FQuartzQuantizationBoundary Boundary;
	Boundary.Quantization = EQuartzCommandQuantization::Beat;
	Boundary.Multiplier = 1.f;
	Boundary.CountingReferencePoint = EQuarztQuantizationReference::BarRelative;

	FOnQuartzCommandEventBP EmptyDelegate;
	UQuartzClockHandle* Handle = BGMClockHandle;
	BGMClockHandle->SetBeatsPerMinute(this, Boundary, EmptyDelegate, Handle, NewBPM);

	UE_LOG(LogExMusic, Log, TEXT("[ExMusicManager] BPM 변경: %.1f"), NewBPM);
}

// ────────────────────────────────────────────────────────────
// Quartz 메트로놈 콜백
// ────────────────────────────────────────────────────────────

void UExMusicManagerSubsystem::HandleBeatEvent(FName ClockName, EQuartzCommandQuantization QuantizationType, int32 NumBars, int32 Beat, float BeatFraction)
{
	CurrentBeatIndex = Beat;
	CurrentBarIndex = NumBars;

	// 비트 이벤트를 ExGameplayEventSubsystem으로 브로드캐스트
	UWorld* World = GetWorld();
	if (World)
	{
		if (UExGameplayEventSubsystem* EventSub = World->GetSubsystem<UExGameplayEventSubsystem>())
		{
			// 매 비트 이벤트
			FExGameplayEventPayload Payload;
			Payload.OptionalValue = static_cast<float>(Beat);
			EventSub->BroadcastEvent(ExMusicTags::Music_Beat, Payload);

			// 강박 이벤트 (4/4 기준 1, 3번째 비트 = 인덱스 0, 2)
			if (Beat == 0 || Beat == 2)
			{
				EventSub->BroadcastEvent(ExMusicTags::Music_Beat_Strong, Payload);
			}
		}
	}

	UE_LOG(LogExMusic, Verbose, TEXT("[ExMusicManager] Beat: %d | Bar: %d"), Beat, NumBars);
}

void UExMusicManagerSubsystem::HandleBarEvent(FName ClockName, EQuartzCommandQuantization QuantizationType, int32 NumBars, int32 Beat, float BeatFraction)
{
	CurrentBarIndex = NumBars;

	// 마디 이벤트를 ExGameplayEventSubsystem으로 브로드캐스트
	UWorld* World = GetWorld();
	if (World)
	{
		if (UExGameplayEventSubsystem* EventSub = World->GetSubsystem<UExGameplayEventSubsystem>())
		{
			FExGameplayEventPayload Payload;
			Payload.OptionalValue = static_cast<float>(NumBars);
			EventSub->BroadcastEvent(ExMusicTags::Music_Bar, Payload);
		}
	}

	UE_LOG(LogExMusic, Verbose, TEXT("[ExMusicManager] ── Bar: %d ──"), NumBars);
}

// ────────────────────────────────────────────────────────────
// 내부 헬퍼 함수
// ────────────────────────────────────────────────────────────

void UExMusicManagerSubsystem::CreateQuartzClock(float BPM, int32 Numerator, int32 Denominator)
{
	UWorld* World = GetWorld();
	check(World);

	UQuartzSubsystem* QuartzSub = UQuartzSubsystem::Get(World);
	if (!ensure(QuartzSub))
	{
		UE_LOG(LogExMusic, Error, TEXT("[ExMusicManager] QuartzSubsystem을 찾을 수 없습니다."));
		return;
	}

	// Clock 설정
	FQuartzClockSettings ClockSettings;
	ClockSettings.TimeSignature.NumBeats = Numerator;
	ClockSettings.TimeSignature.BeatType = static_cast<EQuartzTimeSignatureQuantization>(Denominator);

	// Clock 생성 (이미 존재하면 설정 덮어쓰기)
	BGMClockHandle = QuartzSub->CreateNewClock(this, BGMClockName, ClockSettings, true);

	if (BGMClockHandle)
	{
		// BPM 설정
		FQuartzQuantizationBoundary ImmediateBoundary;
		ImmediateBoundary.Quantization = EQuartzCommandQuantization::None;

		FOnQuartzCommandEventBP EmptyDelegate;
		UQuartzClockHandle* Handle = BGMClockHandle;
		BGMClockHandle->SetBeatsPerMinute(this, ImmediateBoundary, EmptyDelegate, Handle, BPM);

		// Clock 시작
		BGMClockHandle->StartClock(this, Handle);

		UE_LOG(LogExMusic, Log, TEXT("[ExMusicManager] Quartz Clock 생성 완료: %s (BPM: %.1f)"), *BGMClockName.ToString(), BPM);
	}
}

void UExMusicManagerSubsystem::DestroyQuartzClock()
{
	if (!BGMClockHandle)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World)
	{
		UQuartzSubsystem* QuartzSub = UQuartzSubsystem::Get(World);
		if (QuartzSub)
		{
			QuartzSub->DeleteClockByName(this, BGMClockName);
		}
	}

	BGMClockHandle = nullptr;
	UE_LOG(LogExMusic, Log, TEXT("[ExMusicManager] Quartz Clock 삭제 완료"));
}

void UExMusicManagerSubsystem::SubscribeToMetronomeEvents()
{
	if (!ensure(BGMClockHandle))
	{
		return;
	}

	UQuartzClockHandle* Handle = BGMClockHandle;

	// Beat (Quarter Note) 이벤트 구독
	FOnQuartzMetronomeEventBP BeatDelegate;
	BeatDelegate.BindDynamic(this, &UExMusicManagerSubsystem::HandleBeatEvent);
	BGMClockHandle->SubscribeToQuantizationEvent(this, EQuartzCommandQuantization::Beat, BeatDelegate, Handle);

	// Bar 이벤트 구독
	FOnQuartzMetronomeEventBP BarDelegate;
	BarDelegate.BindDynamic(this, &UExMusicManagerSubsystem::HandleBarEvent);
	BGMClockHandle->SubscribeToQuantizationEvent(this, EQuartzCommandQuantization::Bar, BarDelegate, Handle);

	UE_LOG(LogExMusic, Log, TEXT("[ExMusicManager] 메트로놈 이벤트 구독 완료 (Beat + Bar)"));
}

UAudioComponent* UExMusicManagerSubsystem::CreateBGMAudioComponent(USoundBase* InSound)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// 월드에 Audio Component를 스폰
	// 2D 사운드로 재생 (공간 음향 비활성화)
	UAudioComponent* AudioComp = UGameplayStatics::SpawnSound2D(
		World,
		InSound,
		1.0f,  // Volume
		1.0f,  // Pitch
		0.f,   // StartTime
		nullptr, // ConcurrencySettings
		false,   // bPersistAcrossLevelTransition (추후 필요 시 true로 변경)
		false    // bAutoDestroy: 수동 관리
	);

	if (AudioComp)
	{
		// 재생 중지 (PlayQuantized로 다시 시작할 것이므로)
		AudioComp->Stop();

		// 음악 사운드로 표시
		AudioComp->bIsMusic = true;
		AudioComp->bAlwaysPlay = true;
		AudioComp->bIgnoreForFlushing = true;

		UE_LOG(LogExMusic, Log, TEXT("[ExMusicManager] BGM AudioComponent 생성 완료: %s"), *InSound->GetName());
	}

	return AudioComp;
}

// ────────────────────────────────────────────────────────────
// 2단계: Phase 레이어 믹싱
// ────────────────────────────────────────────────────────────

void UExMusicManagerSubsystem::Tick(float DeltaTime)
{
	if (bIsPlaying)
	{
		UpdateLayerVolumes(DeltaTime);
	}
}

TStatId UExMusicManagerSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UExMusicManagerSubsystem, STATGROUP_Tickables);
}

void UExMusicManagerSubsystem::SetPhaseDataAsset(UExMusicPhaseDataAsset* InPhaseData)
{
	PhaseDataAsset = InPhaseData;

	if (PhaseDataAsset)
	{
		UE_LOG(LogExMusic, Log, TEXT("[ExMusicManager] Phase 데이터 에셋 설정: %s (%d개 레이어, %d개 Phase)"),
			*PhaseDataAsset->GetName(),
			PhaseDataAsset->LayerConfigs.Num(),
			PhaseDataAsset->PhasePresets.Num());

		// BGM이 이미 재생 중이면 즉시 레이어 상태 초기화
		if (bIsPlaying)
		{
			InitializeLayerStates();

			if (PhaseDataAsset->InitialPhaseTag.IsValid())
			{
				TransitionToPhase(PhaseDataAsset->InitialPhaseTag);
			}
		}
	}
}

void UExMusicManagerSubsystem::TransitionToPhase(FGameplayTag NewPhaseTag)
{
	if (!PhaseDataAsset)
	{
		UE_LOG(LogExMusic, Warning, TEXT("[ExMusicManager] TransitionToPhase 실패: Phase 데이터 에셋이 설정되지 않았습니다."));
		return;
	}

	if (CurrentMusicPhase == NewPhaseTag)
	{
		UE_LOG(LogExMusic, Verbose, TEXT("[ExMusicManager] 이미 동일한 Phase입니다: %s"), *NewPhaseTag.ToString());
		return;
	}

	const FExMusicPhasePreset* Preset = PhaseDataAsset->FindPreset(NewPhaseTag);
	if (!Preset)
	{
		UE_LOG(LogExMusic, Warning, TEXT("[ExMusicManager] Phase 프리셋을 찾을 수 없습니다: %s"), *NewPhaseTag.ToString());
		return;
	}

	FGameplayTag OldPhase = CurrentMusicPhase;
	CurrentMusicPhase = NewPhaseTag;

	// 프리셋에 정의된 레이어별 목표 볼륨 설정
	for (const auto& LayerVolPair : Preset->LayerVolumes)
	{
		const FName& LayerName = LayerVolPair.Key;
		const float TargetVol = FMath::Clamp(LayerVolPair.Value, 0.f, 1.f);

		LayerTargetVolumes.FindOrAdd(LayerName) = TargetVol;

		// 보간 속도 계산: TransitionDuration동안 변화
		if (Preset->TransitionDuration > KINDA_SMALL_NUMBER)
		{
			const float CurrentVol = LayerCurrentVolumes.FindOrAdd(LayerName);
			const float VolDelta = FMath::Abs(TargetVol - CurrentVol);
			LayerInterpSpeeds.FindOrAdd(LayerName) = (VolDelta > KINDA_SMALL_NUMBER)
				? VolDelta / Preset->TransitionDuration
				: 1.f;  // 변화 없으면 기본 속도
		}
		else
		{
			// 즉시 전환
			LayerCurrentVolumes.FindOrAdd(LayerName) = TargetVol;
			LayerInterpSpeeds.FindOrAdd(LayerName) = 100.f; // 매우 빠른 보간
		}
	}

	UE_LOG(LogExMusic, Log, TEXT("[ExMusicManager] Phase 전환: %s → %s (전환시간: %.1fs)"),
		OldPhase.IsValid() ? *OldPhase.ToString() : TEXT("None"),
		*NewPhaseTag.ToString(),
		Preset->TransitionDuration);
}

void UExMusicManagerSubsystem::SetLayerVolume(FName LayerName, float TargetVolume, float TransitionTime)
{
	const float ClampedVol = FMath::Clamp(TargetVolume, 0.f, 1.f);

	LayerTargetVolumes.FindOrAdd(LayerName) = ClampedVol;

	if (TransitionTime > KINDA_SMALL_NUMBER)
	{
		const float CurrentVol = LayerCurrentVolumes.FindOrAdd(LayerName);
		const float VolDelta = FMath::Abs(ClampedVol - CurrentVol);
		LayerInterpSpeeds.FindOrAdd(LayerName) = (VolDelta > KINDA_SMALL_NUMBER)
			? VolDelta / TransitionTime
			: 1.f;
	}
	else
	{
		LayerCurrentVolumes.FindOrAdd(LayerName) = ClampedVol;
		LayerInterpSpeeds.FindOrAdd(LayerName) = 100.f;
	}

	UE_LOG(LogExMusic, Log, TEXT("[ExMusicManager] 레이어 볼륨 설정: %s → %.2f (전환: %.1fs)"),
		*LayerName.ToString(), ClampedVol, TransitionTime);
}

void UExMusicManagerSubsystem::InitializeLayerStates()
{
	LayerCurrentVolumes.Empty();
	LayerTargetVolumes.Empty();
	LayerInterpSpeeds.Empty();

	if (!PhaseDataAsset)
	{
		return;
	}

	// 레이어 설정 기반으로 초기 볼륨 상태 생성
	for (const FExMusicLayerConfig& LayerConfig : PhaseDataAsset->LayerConfigs)
	{
		const float InitVol = FMath::Clamp(LayerConfig.InitialVolume, 0.f, 1.f);
		LayerCurrentVolumes.Add(LayerConfig.LayerName, InitVol);
		LayerTargetVolumes.Add(LayerConfig.LayerName, InitVol);
		LayerInterpSpeeds.Add(LayerConfig.LayerName, 1.f);

		// 오디오 컴포넌트에 초기값 적용
		if (BaseLayerAudioComp && !LayerConfig.VolumeParameterName.IsNone())
		{
			BaseLayerAudioComp->SetFloatParameter(LayerConfig.VolumeParameterName, InitVol);
		}
	}

	UE_LOG(LogExMusic, Log, TEXT("[ExMusicManager] 레이어 상태 초기화: %d개 레이어"), PhaseDataAsset->LayerConfigs.Num());
}

void UExMusicManagerSubsystem::UpdateLayerVolumes(float DeltaTime)
{
	if (!PhaseDataAsset || !BaseLayerAudioComp)
	{
		return;
	}

	bool bAnyChange = false;

	for (const FExMusicLayerConfig& LayerConfig : PhaseDataAsset->LayerConfigs)
	{
		const FName& LayerName = LayerConfig.LayerName;

		float* CurrentVolPtr = LayerCurrentVolumes.Find(LayerName);
		const float* TargetVolPtr = LayerTargetVolumes.Find(LayerName);
		const float* InterpSpeedPtr = LayerInterpSpeeds.Find(LayerName);

		if (!CurrentVolPtr || !TargetVolPtr || !InterpSpeedPtr)
		{
			continue;
		}

		// 목표 볼륨에 도달했으면 스킵
		if (FMath::IsNearlyEqual(*CurrentVolPtr, *TargetVolPtr, KINDA_SMALL_NUMBER))
		{
			continue;
		}

		// 보간 수행
		const float NewVol = FMath::FInterpConstantTo(*CurrentVolPtr, *TargetVolPtr, DeltaTime, *InterpSpeedPtr);
		*CurrentVolPtr = NewVol;

		// MetaSound 파라미터로 적용
		if (!LayerConfig.VolumeParameterName.IsNone())
		{
			BaseLayerAudioComp->SetFloatParameter(LayerConfig.VolumeParameterName, NewVol);
		}

		bAnyChange = true;
	}
}
