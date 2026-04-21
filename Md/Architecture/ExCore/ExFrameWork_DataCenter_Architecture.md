# ExFrameWork DataCenter 시스템 기술 명세

> **버전:** v1.0  
> **대상 엔진:** Unreal Engine 5  
> **모듈:** ExCore (`ExCoreRuntime`)  
> **작성일:** 2026-04-20  
> **관련 설계 문서:** `Md/Plans/ExFrameWork_DataCenter_System_Plan.md`

---

## 1. 개요

`UExDataCenterSubsystem`을 중심으로 하는 통합 데이터 관리 시스템이다.  
프로젝트 내 11개 이상으로 분산되어 있던 DataAsset을 **Config / Definition / Preset** 3가지 유형으로 체계화하고, 모든 클래스가 단일 인터페이스로 데이터에 접근할 수 있도록 한다.

---

## 2. 구성 파일

### 2.1 Base 추상 클래스 (`ExCoreRuntime/Data/Base/`)

| 파일 | 클래스 | 역할 |
|---|---|---|
| `ExConfigDataAsset.h/.cpp` | `UExConfigDataAsset` | Config(설정형) 최상위 추상 베이스 |
| `ExDefinitionDataAsset.h/.cpp` | `UExDefinitionDataAsset` | Definition(정의형) 최상위 추상 베이스 |
| `ExPresetDataAsset.h/.cpp` | `UExPresetDataAsset` | Preset(프리셋형) 최상위 추상 베이스 |

### 2.2 DataCenter Subsystem (`ExCoreRuntime/Subsystems/`)

| 파일 | 클래스 | 역할 |
|---|---|---|
| `ExDataCenterSubsystem.h/.cpp` | `UExDataCenterSubsystem` | 중앙 등록·조회·해제 싱글톤 |

### 2.3 GameFeature 연동 액션 (`ExCoreRuntime/Actions/`)

| 파일 | 클래스 | 역할 |
|---|---|---|
| `GameFeatureAction_AddExData.h/.cpp` | `UGameFeatureAction_AddExData` | Feature 활성화 시 데이터 자동 등록 |

---

## 3. 데이터 유형 분류 기준

새로운 DataAsset을 생성할 때는 아래 판단 트리를 따른다:

```
"이 데이터는 프로젝트에 인스턴스가 몇 개 필요한가?"
│
├─ "딱 하나. 전역 수치/설정이다" ──────────────→ UExConfigDataAsset 상속
│
└─ "여러 개. 종류만큼 필요하다"
    │
    ├─ "개체 자체를 정의한다 (이것은 무엇인가)" ──→ UExDefinitionDataAsset 상속
    │
    └─ "Definition들의 조합/룰이다" ────────────→ UExPresetDataAsset 상속
```

### 유형별 특성 요약

| 유형 | Base 클래스 | PrimaryAssetType | 인스턴스 수 | 핵심 전략 |
|---|---|---|---|---|
| Config | `UExConfigDataAsset` | `ExConfig` | 모듈당 1개 | USTRUCT 멤버로 설정 통합 |
| Definition | `UExDefinitionDataAsset` | `ExDefinition` | 종류만큼 N개 | DefinitionTag로 식별, 태그 유일성 강제 |
| Preset | `UExPresetDataAsset` | `ExPreset` | 모드·난이도별 M개 | PresetTag + 클래스 복합키로 조회 |

---

## 4. DataCenter 저장소 구조

```
UExDataCenterSubsystem
│
├── ConfigMap
│    TMap<UClass*, UExConfigDataAsset*>
│    클래스 타입을 키로 1:1 매핑
│
├── DefinitionMap
│    TMap<UClass*, TMap<FGameplayTag, UExDefinitionDataAsset*>>
│    복합키: 타입별로 태그 공간 분리 → 동일 클래스 내 태그 유일성 보장
│
└── PresetMap
     TMap<UClass*, TMap<FGameplayTag, UExPresetDataAsset*>>
     복합키: 같은 모드 태그를 다른 Preset 타입(RulePreset, SpawnPreset)이
     공유해도 덮어쓰기 충돌 없음
```

---

## 5. 핵심 API

```cpp
// DataCenter 획득
UExDataCenterSubsystem* DC = GetGameInstance()->GetSubsystem<UExDataCenterSubsystem>();

// Config 조회 (타입 키)
UExRunnerConfigAsset* Config = DC->GetConfig<UExRunnerConfigAsset>();

// Definition 단건 조회 (태그 키)
UExItemDefinition* Item = DC->FindDefinition<UExItemDefinition>(Ex_Item_Coin_Tag);

// Definition 전체 목록 조회
TArray<UExObstacleDefinition*> Obstacles = DC->GetAllDefinitions<UExObstacleDefinition>();

// Preset 조회 (타입 + 모드 태그 복합키)
UExRunnerRulePreset* Preset = DC->GetPreset<UExRunnerRulePreset>(Ex_Mode_Endless_Tag);
```

---

## 6. GameFeature 연동 흐름

```
GameFeature 활성화 (OnGameFeatureActivating)
│
├── UGameFeatureAction_AddExData 발동
│
├── DataCenter->RegisterConfig(ConfigAsset, FeatureName)
├── DataCenter->RegisterDefinition(DefinitionAsset, FeatureName)  ← 목록 순회
├── DataCenter->RegisterPreset(PresetAsset, FeatureName)          ← 목록 순회
│
└── OnDataCenterUpdated 델리게이트 브로드캐스트

GameFeature 비활성화 (OnGameFeatureDeactivating)
│
└── DataCenter->UnregisterByFeature(FeatureName)
     → 해당 Feature가 등록한 모든 Config / Definition / Preset 일괄 해제
     → OnDataCenterUpdated 브로드캐스트
```

**의존성 방향 보장:**  
ExCore는 ExRunnerPlay를 참조하지 않는다. Feature가 자신의 데이터를 DataCenter에 Push하는 방식이므로 Core → Feature 의존성이 발생하지 않는다.

---

## 7. 에셋 명명 규칙

| 유형 | 접두사 패턴 | 예시 |
|---|---|---|
| Config DA | `DA_ExConfig_[모듈]` | `DA_ExConfig_Core`, `DA_ExConfig_Runner` |
| Definition DA | `DA_Ex[타입]_[이름]` | `DA_ExItem_Coin`, `DA_ExObstacle_Gap` |
| Preset DA | `DA_ExPreset_[용도]` | `DA_ExPreset_EndlessMode` |

---

## 8. 필수 개발 규칙

### 8.1 캐싱 규칙 (필수 준수)

```cpp
// ✅ 올바른 방법 — BeginPlay에서 1회 조회 후 TWeakObjectPtr에 캐싱
void AExMyActor::BeginPlay()
{
    Super::BeginPlay();
    if (auto* DC = GetGameInstance()->GetSubsystem<UExDataCenterSubsystem>())
    {
        CachedConfig = DC->GetConfig<UExRunnerConfigAsset>();
    }
}

// ✅ 사용 시 IsValid 체크
if (CachedConfig.IsValid())
{
    float Speed = CachedConfig->GameMode.RunSpeed;
}

// ❌ 금지 — Tick에서 매 프레임 DataCenter 직접 조회
void AExMyActor::Tick(float DeltaTime)
{
    auto* DC = GetGameInstance()->GetSubsystem<UExDataCenterSubsystem>();
    float Speed = DC->GetConfig<UExRunnerConfigAsset>()->GameMode.RunSpeed; // 금지!
}

// ❌ 금지 — Raw Pointer 캐싱 (GameFeature 비활성화 시 Stale 참조 위험)
UExRunnerConfigAsset* CachedConfig; // Raw Pointer 금지!
```

### 8.2 IsDataValid 검증 (저장 시 자동 실행)

Base 클래스들은 `IsDataValid`를 오버라이드하여 저장 시점에 데이터 정합성을 검증한다:
- **Config**: 필수 수치 범위(확률 0~1, 거리 양수 등)
- **Definition**: `DefinitionTag` 필수 지정, 필수 레퍼런스 nullptr 금지
- **Preset**: `PresetTag` 필수 지정, Definition 배열 nullptr 금지

서브클래스에서 반드시 `Super::IsDataValid(Context)` 를 호출한 뒤 추가 검증을 작성한다.

### 8.3 Core/Feature 분리 원칙

- Base 클래스 3개와 DataCenter Subsystem은 **ExCore에만** 배치한다
- **ExCore는 ExRunnerPlay를 참조하지 않는다**
- 러너 게임 전용 구현체(예: `UExObstacleDefinition`)는 **ExRunnerPlay**에 배치한다
- ExCore에는 장르를 유추할 수 없는 범용 데이터만 배치한다

---

## 9. 확장 방침

| 조건 | 전환 내용 |
|---|---|
| Definition DA가 50개 이상 | 매니페스트 배열을 `TSoftObjectPtr` 또는 `FPrimaryAssetId` 기반으로 전환, 비동기 로드 구조 도입 |
| DA 전체 50개 이상 | DataCenter 내부 저장소를 `UAssetManager` 기반으로 교체 (외부 API 시그니처 불변) |
| 팀 확장 | Config DA를 분야별 분리 (예: `DA_Runner_Gameplay`, `DA_Runner_LevelDesign`) |
| 데이터 50개 이상 | `EditorUtilityWidget` 기반 "Ex Data Browser" 커스텀 에디터 제작 검토 |

---

## 10. 마이그레이션 순서

기존 DataAsset 클래스들은 아래 순서로 점진적으로 마이그레이션한다:

1. **1단계 (완료)**: Base 클래스 3개 + DataCenter Subsystem + GFAction 인프라 구축
2. **2단계**: 기존 Config 클래스들을 `USTRUCT`로 변환 → `UExRunnerConfigAsset` 통합
   - `DefaultEngine.ini`에 **CoreRedirects** 작성 필수
   - 전환 후 관련 에셋 전체 일괄 리세이브(Resave Packages) 필수
3. **3단계**: `UExItemDefinition`, `UExObstacleDefinition`의 Base를 `UExDefinitionDataAsset`으로 변경  
   `ExRunnerRuleConfig`, `ExRunnerItemSpawnTable`의 Base를 `UExPresetDataAsset`으로 변경

---

## 11. 추가 마이그레이션 후보 분석 (ExRunnerPlay 컴포넌트)

> **분석일:** 2026-04-21  
> **상태:** 검토 대기 — 수정 전 승인 필요  
> **대상 플러그인:** `ExRunnerPlay`

Phase 1~5 마이그레이션 완료 후에도 ExRunnerPlay 컴포넌트들의 멤버 변수 중 DataCenter Config로 이전 가능한 항목들이 남아 있다.

---

### 11.1 발견된 이슈: UExGameModeDataSet 이중 문제

**파일:** `Source/ExFrameWork/Data/Modes/ExGameModeDataSet.h` (ExCore 모듈)

`UExGameModeDataSet`는 두 가지 문제를 동시에 가진다.

| 문제 | 내용 |
|---|---|
| **위치 위반** | ExCore 모듈에 있으나 Runner 전용 설정값(`MaxRunnerYawAngle`, `RunnerLookSensitivity`, `LookInterpSpeed`, `SwipeActivationPercentage`, `AutoRunActionCooldown`, `JumpYawPredictionWeight`)을 포함 → Core/Feature 분리 원칙 위반 |
| **DataCenter 외부** | `UDataAsset`을 직접 상속하여 DataCenter 3-Base 시스템 밖에 존재 |

현재 `UExRunnerInputComponent`와 `UExRunnerMovementComponent`가 이 클래스를 폴백 참조로 직접 포인터로 들고 있다.

```cpp
// ExRunnerInputComponent.h — 현재 상태 (개선 대상)
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ExInput|Runner|Settings")
class UExGameModeDataSet* GameModeDataSet;  // DataCenter 외부 직접 참조

// ExRunnerMovementComponent.h — 현재 상태 (개선 대상)
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Runner|Look")
class UExGameModeDataSet* GameModeDataSet;  // DataCenter 외부 직접 참조
```

`UExGameModeDataSet`의 멤버 중 범용적인 것(`MaxScore`, `NumRounds`, `ContainerPawnClass`, `LocalPlayerClass`)은 ExCore 수준 Config로, Runner 특화 항목들은 `UExRunnerConfig`로 통합하는 것이 바람직하다.

---

### 11.2 컴포넌트별 Config 후보 멤버 목록

#### UExBeatSyncComponent
**파일:** `ExRunnerPlayRuntime/Components/ExBeatSyncComponent.h`  
**현재 상태:** 세 멤버 모두 컴포넌트에 `UPROPERTY(EditAnywhere)`로 직접 노출

| 멤버 변수 | 타입 | 기본값 | 제안 구조체 |
|---|---|---|---|
| `SpawnProbabilityPerBeat` | `float` | `0.5f` | `FExBeatSyncSettings` |
| `StrongBeatBonus` | `float` | `0.2f` | `FExBeatSyncSettings` |
| `bBeatSyncEnabled` | `bool` | `true` | `FExBeatSyncSettings` |

---

#### UExChunkSpawner
**파일:** `ExRunnerPlayRuntime/Components/ExChunkSpawner.h`  
**현재 상태:** `TWeakObjectPtr<UExRunnerConfig> RunnerConfig` 참조는 이미 존재하나, 아래 설정값들은 여전히 컴포넌트 자체 멤버로 남아있음 (부분 마이그레이션 상태)

| 멤버 변수 | 타입 | 기본값 | 제안 구조체 |
|---|---|---|---|
| `bUsePooling` | `bool` | `false` | `FExChunkSpawnSettings` |
| `InitialPoolSize` | `int32` | `5` | `FExChunkSpawnSettings` |
| `SpawnStartX` | `float` | `0.f` | `FExChunkSpawnSettings` |
| `ChunkSpacing` | `float` | `1000.f` | `FExChunkSpawnSettings` |
| `MaxActiveChunks` | `int32` | `10` | `FExChunkSpawnSettings` |

> 참고: `ChunkClass`(`TSubclassOf<AExFloorChunk>`)는 에디터 배치 설정으로 Config와 성격이 달라 제외.

---

#### UExRunnerMovementComponent
**파일:** `ExRunnerPlayRuntime/Components/ExRunnerMovementComponent.h`

| 멤버 변수 | 타입 | 기본값 | 제안 구조체 |
|---|---|---|---|
| `LaneWidth` | `float` | `100.0f` | `FExMovementSettings` |
| `LaneChangeSpeed` | `float` | `10.0f` | `FExMovementSettings` |

> 참고: `GameModeDataSet` 포인터는 11.1에서 설명한 이중 문제 항목으로 별도 검토.

---

#### UExRunnerInputComponent
**파일:** `ExRunnerPlayRuntime/Components/ExRunnerInputComponent.h`

| 멤버 변수 | 타입 | 기본값 | 제안 구조체 | 비고 |
|---|---|---|---|---|
| `RunnerLookSensitivity` | `float` | `0.5f` | `FExInputSettings` | `UExGameModeDataSet::RunnerLookSensitivity`와 중복 |
| `DefaultInputMode` | `EExRunnerInputMode` | `Manual` | `FExInputSettings` | |

> 참고: `JumpAction`, `SlideAction` 등 `UInputAction*` 참조는 에디터 바인딩 데이터이므로 제외.  
> `GameModeDataSet` 포인터는 11.1 항목으로 별도 검토.

---

#### UExObstacleManager
**파일:** `ExRunnerPlayRuntime/Components/ExObstacleManager.h`

| 멤버 변수 | 타입 | 기본값 | 제안 구조체 | 비고 |
|---|---|---|---|---|
| `bSuppressDefaultChunkSpawn` | `bool` | `false` | `FExObstacleSpawnSettings` (기존) | 비트 동기화 활성화 시 청크 스폰 트리거 억제 플래그. 런타임 동적 변경이 필요한지 여부 확인 필요 |

> 참고: `SpawnStrategies` (`TMap<EExObstacleType, UExObstacleSpawnStrategy*>`)는 Strategy Pattern 인스턴스이므로 DataCenter 대상 아님.

---

### 11.3 제안 통합 방향

모든 후보를 `UExRunnerConfig` 단일 Config DA에 USTRUCT 형태로 추가하는 방식을 권장한다.

```
UExRunnerConfig (UExConfigDataAsset)
├── FExCurveSettings          Curve          [기존 완료]
├── FExObstacleSpawnSettings  ObstacleSpawn  [기존 완료 + bSuppressDefaultChunkSpawn 추가 검토]
├── FExBeatSyncSettings       BeatSync       [신규]
├── FExChunkSpawnSettings     ChunkSpawn     [신규]
├── FExMovementSettings       Movement       [신규]
└── FExInputSettings          Input          [신규]
```

`UExGameModeDataSet`의 Runner 전용 멤버들(`MaxRunnerYawAngle`, `LookInterpSpeed`, `SwipeActivationPercentage`, `AutoRunActionCooldown`, `JumpYawPredictionWeight`)도 `FExInputSettings` 또는 별도 `FExGameplaySettings`로 통합을 검토한다.

---

### 11.4 마이그레이션 제외 항목 (이유 포함)

| 클래스 | 멤버 | 제외 이유 |
|---|---|---|
| `UExChunkSpawner` | `ChunkClass` (`TSubclassOf`) | 에디터 배치 레퍼런스, Config 성격 아님 |
| `UExObstacleManager` | `SpawnStrategies` | Strategy Pattern 인스턴스, 설정값 아님 |
| `UExRunnerInputComponent` | `JumpAction`, `SlideAction` 등 | Enhanced Input 바인딩, Config 성격 아님 |
| `AExRunnerGameState` | `CurrentPathDistance`, `RemainingTimeSeconds` 등 | 런타임 게임 진행 상태 |
| `UExRunnerStatComponent` | 현재 스탯들 | 플레이어별 동적 상태 |
| `AExFloorChunk` | `KillZ`, 커브 캐시 등 | 청크 인스턴스 런타임 상태 |
