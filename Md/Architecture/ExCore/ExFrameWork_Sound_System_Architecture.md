# ExFrameWork 사운드 시스템 아키텍처

> **버전**: 1.0  
> **엔진**: Unreal Engine 5.7.3  
> **최종 수정**: 2026-03-20  
> **모듈**: `ExCoreRuntime` (범용) / `ExRunnerPlayRuntime` (러너 전용)

---

## 1. 개요

ExFrameWork 사운드 시스템은 UE5의 **Quartz Clock** (비트 정밀 타이밍)과 **MetaSound** (DSP 그래프 기반 오디오)를 결합하여, 게임플레이와 음악이 실시간으로 동기화되는 **적응형 BGM 시스템**을 제공합니다.

### 핵심 설계 원칙

| 원칙 | 설명 |
|------|------|
| **데이터 드리븐** | Phase별 레이어 볼륨은 `UExMusicPhaseDataAsset`으로 에디터에서 구성 |
| **이벤트 드리븐** | 비트/마디 이벤트를 `ExGameplayEventSubsystem`으로 브로드캐스트하여 느슨한 결합 유지 |
| **모듈 분리** | 범용 음악 엔진(`ExCore`)과 게임 특화 로직(`ExRunnerPlay`)을 분리 |
| **실시간 보간** | Phase 전환 시 Tick 기반 `FInterpConstantTo`로 부드러운 레이어 크로스페이드 |

---

## 2. 시스템 아키텍처 다이어그램

```
┌─────────────────────────────────────────────────────────────────┐
│                      ExRunnerPlayRuntime                        │
│                                                                 │
│  ┌──────────────────────────┐                                   │
│  │   AExRunnerGameMode      │                                   │
│  │  ┌────────────────────┐  │      ┌──────────────────────┐     │
│  │  │ BGMSound           │──┼─────▶│ ExBeatSyncComponent  │     │
│  │  │ DefaultBPM         │  │      │ (3단계 예정)          │     │
│  │  │ MusicPhaseData     │  │      └──────────────────────┘     │
│  │  │ SetRunnerPhase()   │  │                                   │
│  │  └───────┬────────────┘  │                                   │
│  └──────────┼───────────────┘                                   │
└─────────────┼───────────────────────────────────────────────────┘
              │ SetPhaseDataAsset() / StartBGM() / TransitionToPhase()
              ▼
┌─────────────────────────────────────────────────────────────────┐
│                        ExCoreRuntime                            │
│                                                                 │
│  ┌──────────────────────────────────────────────────┐           │
│  │        UExMusicManagerSubsystem                   │           │
│  │        (UWorldSubsystem + FTickableGameObject)    │           │
│  │                                                   │           │
│  │  ┌─────────────┐   ┌──────────────────────┐      │           │
│  │  │ Quartz Clock│   │ UAudioComponent      │      │           │
│  │  │ (ExBGMClock)│   │ (BaseLayerAudioComp) │      │           │
│  │  │  ┌────────┐ │   │  ┌────────────────┐  │      │           │
│  │  │  │BPM제어 │ │   │  │ PlayQuantized  │  │      │           │
│  │  │  │비트구독│ │   │  │ SetFloatParam  │  │      │           │
│  │  │  │마디구독│ │   │  │ FadeOut/Stop   │  │      │           │
│  │  │  └────────┘ │   │  └────────────────┘  │      │           │
│  │  └─────────────┘   └──────────────────────┘      │           │
│  │                                                   │           │
│  │  ┌──────────────────────────────────────┐         │           │
│  │  │ Tick 레이어 보간 엔진                 │         │           │
│  │  │ LayerCurrentVolumes ──interpolate──▶  │         │           │
│  │  │ LayerTargetVolumes   FInterpConstantTo│         │           │
│  │  │ LayerInterpSpeeds                     │         │           │
│  │  └──────────────────────────────────────┘         │           │
│  └──────────────────────────────────────────────────┘           │
│                                                                 │
│  ┌──────────────────────┐   ┌──────────────────────────┐        │
│  │ ExMusicTags           │   │ ExMusicPhaseDataAsset    │        │
│  │ Music_Beat            │   │ LayerConfigs[]           │        │
│  │ Music_Bar             │   │ PhasePresets[]           │        │
│  │ Music_Beat_Strong     │   │ InitialPhaseTag          │        │
│  │ Music_Phase_Warmup    │   │ FindPreset(Tag)          │        │
│  │ Music_Phase_Running   │   └──────────────────────────┘        │
│  │ Music_Phase_Climax    │                                       │
│  │ Music_Phase_Cooldown  │   ┌──────────────────────────┐        │
│  └──────────────────────┘   │ ExGameplayEventSubsystem  │        │
│                              │ BroadcastEvent(BeatTag)   │        │
│                              └──────────────────────────┘        │
└─────────────────────────────────────────────────────────────────┘
              │
              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    UE5 Engine Layer                              │
│                                                                 │
│  ┌────────────────┐  ┌──────────────┐  ┌────────────────────┐   │
│  │ UQuartzSubsystem│  │ UAudioComponent│  │ MetaSound Source  │   │
│  │ CreateNewClock  │  │ PlayQuantized│  │ (DSP 그래프)       │   │
│  │ DeleteClockByName│  │ SetFloatParam│  │ WavePlayer, Gain  │   │
│  └────────────────┘  └──────────────┘  │ Mixer, Filter 등  │   │
│                                         └────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. 핵심 클래스 상세

### 3.1 UExMusicManagerSubsystem

**위치**: `ExCore/Source/ExCoreRuntime/Subsystems/ExMusicManagerSubsystem.h/.cpp`  
**상속**: `UWorldSubsystem` + `FTickableGameObject`

게임 월드 수준에서 BGM의 모든 동작을 관리하는 중앙 서브시스템입니다.

#### Public API

| 함수 | 카테고리 | 설명 |
|------|----------|------|
| `StartBGM(Sound, BPM, Numerator, Denominator)` | 재생 제어 | Quartz Clock 생성 → PlayQuantized → 메트로놈 구독 |
| `StopBGM(FadeOutDuration)` | 재생 제어 | 페이드아웃 → Clock/AudioComp 정리 |
| `PauseBGM(bPause)` | 재생 제어 | Clock + AudioComp 일시정지/재개 |
| `SetBPM(NewBPM)` | BPM 제어 | 비트 경계에서 BPM 변경 |
| `SetPhaseDataAsset(Data)` | Phase 믹싱 | 레이어 구성/Phase 프리셋 설정 |
| `TransitionToPhase(Tag)` | Phase 믹싱 | Phase 전환 → 레이어 볼륨 보간 시작 |
| `SetLayerVolume(Name, Vol, Time)` | Phase 믹싱 | 개별 레이어 직접 제어 |
| `GetCurrentBPM()` | 상태 조회 | 현재 BPM |
| `GetCurrentBeatIndex()` | 상태 조회 | 현재 비트 인덱스 (0~N-1) |
| `GetCurrentBarIndex()` | 상태 조회 | 현재 마디 인덱스 |
| `GetCurrentMusicPhase()` | 상태 조회 | 현재 Phase 태그 |

#### 내부 동작 흐름

```
StartBGM() 호출
    │
    ├─ 1. CreateQuartzClock(BPM, Numerator, Denominator)
    │      └─ UQuartzSubsystem::CreateNewClock → BGMClockHandle
    │      └─ SetBeatsPerMinute → StartClock
    │
    ├─ 2. SubscribeToMetronomeEvents()
    │      └─ SubscribeToQuantizationEvent(Beat) → HandleBeatEvent()
    │      └─ SubscribeToQuantizationEvent(Bar)  → HandleBarEvent()
    │
    ├─ 3. CreateBGMAudioComponent(Sound)
    │      └─ SpawnSound2D → Stop (대기)
    │
    ├─ 4. PlayQuantized(ClockHandle, QuantBoundary)
    │      └─ 다음 비트 경계에서 재생 시작
    │
    ├─ 5. InitializeLayerStates()
    │      └─ PhaseDataAsset → LayerConfigs → 초기 볼륨 설정
    │      └─ SetFloatParameter(VolumeParamName, InitialVolume)
    │
    └─ 6. TransitionToPhase(InitialPhaseTag)  (선택)
           └─ Phase 프리셋의 레이어 볼륨 목표 설정
```

#### Tick 보간 엔진

```cpp
// 매 프레임 UpdateLayerVolumes(DeltaTime) 호출
for (각 레이어)
{
    CurrentVol = FMath::FInterpConstantTo(CurrentVol, TargetVol, DeltaTime, InterpSpeed);
    AudioComp->SetFloatParameter(VolumeParamName, CurrentVol);
}
```

- **InterpSpeed** = |TargetVol - CurrentVol| / TransitionDuration
- `FInterpConstantTo`: 일정 속도 보간 (선형, 시간 정확)
- Phase 전환마다 새로운 TargetVol과 InterpSpeed가 설정됨

---

### 3.2 UExMusicPhaseDataAsset

**위치**: `ExCore/Source/ExCoreRuntime/Data/ExMusicPhaseDataAsset.h`  
**상속**: `UDataAsset`

에디터에서 데이터 드리븐 방식으로 Phase별 레이어 구성을 정의합니다.

#### 데이터 구조

```
UExMusicPhaseDataAsset
├── LayerConfigs[]: FExMusicLayerConfig
│   ├── LayerName           (FName: "Base", "Melody", "Percussion", "Intensity")
│   ├── VolumeParameterName (FName: MetaSound Graph Input 이름과 1:1 매핑)
│   └── InitialVolume       (float: 0.0~1.0)
│
├── PhasePresets[]: FExMusicPhasePreset
│   ├── PhaseTag            (FGameplayTag: Ex.Music.Phase.Running)
│   ├── LayerVolumes        (TMap<FName, float>: 레이어별 목표 볼륨)
│   └── TransitionDuration  (float: 전환 시간 초)
│
└── InitialPhaseTag         (FGameplayTag: 게임 시작 시 자동 적용)
```

#### Phase 전환 예시 데이터

```
Phase: Warmup        →  Phase: Running       →  Phase: Climax
Base:       1.0          Base:       1.0          Base:       1.0
Melody:     0.0          Melody:     0.8          Melody:     1.0
Percussion: 0.0          Percussion: 0.5          Percussion: 1.0
Intensity:  0.0          Intensity:  0.0          Intensity:  0.8
Duration:   -            Duration:   2.0s         Duration:   1.5s
```

---

### 3.3 ExMusicTags

**위치**: `ExCore/Source/ExCoreRuntime/Tags/ExMusicTags.h/.cpp`

| 태그 | 용도 |
|------|------|
| `Ex.Music.Beat` | 매 비트 이벤트 |
| `Ex.Music.Bar` | 매 마디 이벤트 |
| `Ex.Music.Beat.Strong` | 강박 이벤트 (4/4 기준 1, 3번째 비트) |
| `Ex.Music.Phase.Warmup` | 워밍업 Phase |
| `Ex.Music.Phase.Running` | 달리기 Phase |
| `Ex.Music.Phase.Climax` | 클라이맥스 Phase |
| `Ex.Music.Phase.Cooldown` | 쿨다운 Phase |

---

## 4. MetaSound 연동 전략

> **DataCenter 3-Base 체계 편입 검토 결과 (2026-04-22):**  
> `UExMusicPhaseDataAsset`은 `UDataAsset`을 직접 상속하여 DataCenter 외부에 존재한다. 그러나 이 에셋은 `UExBGMTrackDataAsset`의 멤버로 직접 참조되며(곡별 1:1 관계), DataCenter의 태그 기반 조회 패턴이 필요하지 않다. **현재 구조를 유지하며, DataCenter에 편입하지 않는다.**

### 4.1 MetaSound Source ↔ C++ 파라미터 매핑

MetaSound Source의 **Graph Input**(노출 파라미터)과 C++의 `SetFloatParameter`가 **이름 기반**으로 연결됩니다.

```
[MetaSound Source: MS_BGM_Runner]
    │
    ├── Graph Input: "LayerVolume_Base"      ← C++: SetFloatParameter("LayerVolume_Base", 1.0)
    ├── Graph Input: "LayerVolume_Melody"    ← C++: SetFloatParameter("LayerVolume_Melody", 0.8)
    ├── Graph Input: "LayerVolume_Percussion"← C++: SetFloatParameter("LayerVolume_Percussion", 0.5)
    └── Graph Input: "LayerVolume_Intensity" ← C++: SetFloatParameter("LayerVolume_Intensity", 0.0)
```

### 4.2 권장 MetaSound 그래프 구성 (레이어 믹싱)

```
┌──────────────┐     ┌──────┐     ┌────────────┐
│ Wave Player 1│────▶│Gain 1│────▶│            │
│ (Base.wav)   │     │ ↑Vol │     │            │
└──────────────┘     └──────┘     │            │
                                   │ Audio      │     ┌────────┐
┌──────────────┐     ┌──────┐     │ Mixer      │────▶│ Output │
│ Wave Player 2│────▶│Gain 2│────▶│            │     └────────┘
│ (Melody.wav) │     │ ↑Vol │     │            │
└──────────────┘     └──────┘     │            │
                                   │            │
┌──────────────┐     ┌──────┐     │            │
│ Wave Player 3│────▶│Gain 3│────▶│            │
│ (Perc.wav)   │     │ ↑Vol │     │            │
└──────────────┘     └──────┘     │            │
                                   │            │
┌──────────────┐     ┌──────┐     │            │
│ Wave Player 4│────▶│Gain 4│────▶│            │
│ (Intensity)  │     │ ↑Vol │     └────────────┘
└──────────────┘     └──────┘

각 Gain의 Gain Amount ← 대응하는 Graph Input (LayerVolume_xxx)
모든 Wave Player: Loop = true
```

### 4.3 MetaSound에서 사용 가능한 이펙트 노드 (UE 5.7.3 소스 확인)

| 카테고리 | 노드 | 용도 |
|----------|------|------|
| 리버브 | Plate Reverb | 공간감 부여 |
| 딜레이 | Delay, Grain Delay | 에코, 그래뉼러 효과 |
| 모듈레이션 | Flanger, Ring Mod | 스윕, 변조 사운드 |
| 다이나믹 | Compressor, Limiter | 볼륨 균일화, 피킹 방지 |
| 디스토션 | Bitcrusher | 로파이/레트로 효과 |
| 필터 | Basic Filters (LP/HP/BP/Notch), Dynamic Filter, Band Splitter | 주파수 대역 제어 |
| 엔벨로프 | AD, ADSR, Envelope Follower | 볼륨 자동화 |
| 트리거 | Trigger Sequence, Trigger Repeat, Trigger Delay, Trigger Counter | 순차/반복/지연 재생 |
| 믹싱 | Crossfade, Mixer, Gain, Mid-Side | 크로스페이드, 볼륨 |
| 유틸리티 | BPM To Seconds, MIDI To Freq, Map Range, InterpTo, Random | 수학/변환 |

---

## 5. ExRunnerGameMode 연동 전략

### 5.1 생명주기

```
AExRunnerGameMode::BeginPlay()
    └─ (이벤트 구독)

OnMatchStarted_Implementation()
    └─ StartRunnerGame()
        ├─ ChunkSpawner/ObstacleManager 초기화
        ├─ MusicMgr->SetPhaseDataAsset(MusicPhaseData)
        └─ MusicMgr->StartBGM(BGMSound, DefaultBPM)
            └─ InitialPhaseTag 자동 적용 (Warmup)

인게임 Phase 변화 (BlueprintCallable)
    └─ SetRunnerPhase(NewPhaseTag)
        └─ MusicMgr->TransitionToPhase(NewPhaseTag)

OnMatchEnded_Implementation()
    └─ StopRunnerGame()
        └─ MusicMgr->StopBGM(1.5f)  // 1.5초 페이드아웃
```

### 5.2 Phase 전환 타이밍 전략

| 게임 이벤트 | Phase 전환 | 음악 변화 |
|-------------|-----------|-----------|
| 매치 시작 | → Warmup | Base 레이어만 재생 |
| 속도/난이도 증가 | → Running | Melody + Percussion 추가 |
| 보스/위기 구간 | → Climax | 전 레이어 최대 + Intensity |
| 매치 종료 접근 | → Cooldown | 점진적 레이어 감소 |

---

## 6. 의존성 매트릭스

```
ExCoreRuntime.Build.cs 의존성:
  ├── AudioMixer        ← Quartz Clock API
  ├── GameplayTags      ← FGameplayTag
  └── ModularGameplay   ← 기존 의존성

ExRunnerPlayRuntime → ExCoreRuntime:
  ├── ExMusicManagerSubsystem.h
  └── ExMusicPhaseDataAsset.h (간접)
```

### 모듈 경계 원칙

| 모듈 | 책임 | 위치 |
|------|------|------|
| `ExCoreRuntime` | 범용 음악 엔진 (Quartz, 레이어 보간, 이벤트) | 다른 게임 모드에서도 재사용 |
| `ExRunnerPlayRuntime` | 러너 게임 특화 (Phase 전환 시점, 비트-장애물 동기화) | 러너 전용 로직 |
| MetaSound Assets | DSP 그래프, 이펙트, 레이어 믹싱 | 에디터 Content |

---

## 7. 개발 로드맵

| 단계 | 상태 | 내용 |
|------|------|------|
| **1단계**: 기본 BGM 루프 | ✅ 완료 | Quartz Clock + PlayQuantized + Beat/Bar 이벤트 |
| **2단계**: Phase 레이어 믹싱 | ✅ 완료 | DataAsset + Tick 보간 + Phase 전환 |
| **3단계**: 비트-장애물 동기화 | 🔜 예정 | ExBeatSyncComponent → 비트에 맞춰 장애물 스폰 |

### 3단계 설계 방향 (예정)

```
ExBeatSyncComponent (ExRunnerPlayRuntime)
├─ Music_Beat 이벤트 수신
├─ 일정 비트마다 장애물 스폰 트리거
├─ Phase별 스폰 빈도/패턴 변경
└─ ExObstacleManager와 인터페이스
```

---

## 8. 트러블슈팅 가이드

### Q: PlayQuantized에서 사운드가 안 나옴
- MetaSound Source의 Graph Input 이름과 `VolumeParameterName`이 일치하는지 확인
- Wave Player의 **Loop = true** 설정 확인
- `On Play` → Wave Player `Play` → Output 연결 확인

### Q: Phase 전환이 안 됨
- `SetPhaseDataAsset()`이 `StartBGM()` 전에 호출되었는지 확인
- `ExMusicTags`의 태그 이름이 `PhasePresets[].PhaseTag`와 일치하는지 확인
- Output Log에서 `LogExMusic` 필터로 전환 로그 확인

### Q: 비트 이벤트가 안 옴
- `SubscribeToQuantizationEvent` 호출 여부 (StartBGM 내부에서 자동 호출)
- `ExGameplayEventSubsystem` 구독이 올바르게 되었는지 확인

### Q: FQuartzQuantizationBoundary 컴파일 에러
- UE 5.7.3 기준: `CountingReferencePoint` 멤버 사용 (`CountingDirection` 아님!)
- `PlayQuantized`의 ClockHandle 인자는 `UQuartzClockHandle*&` → `TObjectPtr`에서 로우 포인터로 변환 필요
