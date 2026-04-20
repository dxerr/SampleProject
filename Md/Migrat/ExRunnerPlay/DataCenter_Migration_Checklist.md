# ExRunnerPlay DataCenter 마이그레이션 체크리스트

> **대상 플러그인:** `ExRunnerPlay`
> **마이그레이션 목표:** 기존 독립 DataAsset 클래스들을 DataCenter 3-Base 시스템으로 이전
> **통합 방침:** 통합 가능한 구조는 무조건 통합 처리
> **원칙:** 기존 에셋(.uasset) 참조를 유지하면서 점진적으로 이전 (CoreRedirects 활용)
> **작성일:** 2026-04-20

---

## 확정된 통합 방침

| # | 항목 | 결정 내용 |
|---|---|---|
| 1 | `UExItemDefinition.ItemTag` | **`DefinitionTag`로 이름 변경** (CoreRedirects 처리). 중복 멤버 없이 단일 키로 통합 |
| 2 | `UExCurveConfig` + `UExObstacleSpawnConfig` | **`UExRunnerConfig` 1개로 통합** — 각각 USTRUCT(`FExCurveSettings`, `FExObstacleSpawnSettings`)로 변환 후 내부 멤버로 포함 |

---

## 📋 현황 파악 — 기존 DataAsset 클래스 목록

### ExCore 모듈에 잘못 위치한 클래스 (ExRunnerPlay로 이동 대상)

| 클래스 | 현재 위치 | 마이그레이션 방향 |
|---|---|---|
| `UExObstacleDefinition` | `ExCore/Data/ExObstacleDefinition.h` | `UExDefinitionDataAsset` 상속 + **ExRunnerPlay로 이동** |

---

### 마이그레이션 대상 전체 목록

#### 🔵 Config 계열 → `UExRunnerConfig` 1개로 통합

| 기존 클래스 | 통합 결과 | 주요 멤버 |
|---|---|---|
| `UExCurveConfig` | `FExCurveSettings` USTRUCT로 변환 | `FixedCurveRadius`, `SlopePitchAngle`, `SlopeTriggerCount`, `MinStraightChunks`, `MaxStraightChunks`, `CurveProbabilityBase`, `CurveProbabilityGrowth`, `WorldBoundsX`, `WorldBoundsY`, `SplineSegmentCount`, `CharacterRotationInterpSpeed` |
| `UExObstacleSpawnConfig` | `FExObstacleSpawnSettings` USTRUCT로 변환 | `SpawnProbability`, `MinSafeDistance`, `DefaultRunSpeed`, `MaxActiveObstacles`, `bUsePooling` |
| *(통합 결과)* | **`UExRunnerConfig`** (새 파일) | 위 두 USTRUCT를 멤버로 포함 |

#### 🟡 Definition 계열 → `UExDefinitionDataAsset` 상속

| 클래스 | 변경 내용 |
|---|---|
| `UExItemDefinition` | `UPrimaryDataAsset` → `UExDefinitionDataAsset`. `ItemTag` → `DefinitionTag` 이름 변경 |
| `UExObstacleDefinition` | `UDataAsset` → `UExDefinitionDataAsset`. ExCore→ExRunnerPlay 이동. `DefinitionTag` 상속으로 자동 포함 |

#### 🟢 Preset 계열 → `UExPresetDataAsset` 상속

| 클래스 | 변경 내용 |
|---|---|
| `UExRunnerRuleConfig` | `UDataAsset` → `UExPresetDataAsset`. `PresetTag` 상속으로 자동 포함 |
| `UExRunnerItemSpawnTable` | `UDataAsset` → `UExPresetDataAsset`. `PresetTag` 상속으로 자동 포함 |

---

### 관련 구조체 (변경 없음)

| 구조체 | 위치 | 비고 |
|---|---|---|
| `FExItemSpawnEntry` | `ExCore/Struct/Items/` | 변경 없음 — ExCore 위치 유지 |

---

## ✅ 마이그레이션 체크리스트

진행 상태: `[ ]` 미완 / `[/]` 진행 중 / `[x]` 완료

---

### Phase 1 — 인프라 확인 *(완료)*

- [x] `UExConfigDataAsset` 베이스 클래스 생성 (`ExCore/Data/Base/`)
- [x] `UExDefinitionDataAsset` 베이스 클래스 생성 (`ExCore/Data/Base/`)
- [x] `UExPresetDataAsset` 베이스 클래스 생성 (`ExCore/Data/Base/`)
- [x] `UExDataCenterSubsystem` 구현 (`ExCore/Subsystems/`)
- [x] `UGameFeatureAction_AddExData` 구현 (`ExCore/Actions/`)

---

### Phase 2 — Config 통합 신규 클래스 생성 *(완료)*

> `UExCurveConfig`, `UExObstacleSpawnConfig` 두 클래스를 하나의 `UExRunnerConfig`로 통합

#### 2-A. `FExCurveSettings` USTRUCT 생성

- [x] `ExRunnerPlay/Struct/FExCurveSettings.h` 신규 생성
  - 기존 `UExCurveConfig`의 모든 멤버를 USTRUCT로 이전
  - `GetCurveProbability()` 인라인 함수 포함 유지
- [x] 빌드 검증

#### 2-B. `FExObstacleSpawnSettings` USTRUCT 생성

- [x] `ExRunnerPlay/Struct/FExObstacleSpawnSettings.h` 신규 생성
  - 기존 `UExObstacleSpawnConfig`의 모든 멤버를 USTRUCT로 이전

#### 2-C. `UExRunnerConfig` 신규 생성 (`UExConfigDataAsset` 상속)

- [x] `ExRunnerPlay/Data/ExRunnerConfig.h/.cpp` 신규 생성
  - `UExConfigDataAsset` 상속
  - `UPROPERTY FExCurveSettings Curve` 멤버
  - `UPROPERTY FExObstacleSpawnSettings ObstacleSpawn` 멤버
- [x] `IsDataValid` 구현
  - `Curve.FixedCurveRadius > 0` 검증
  - `Curve.MinStraightChunks < Curve.MaxStraightChunks` 검증
  - `ObstacleSpawn.SpawnProbability` 범위(0~1) 검증
- [x] 빌드 검증

#### 2-D. 기존 참조 교체

- [x] `AExRunnerGameMode` — `UExCurveConfig* CurveConfig` 멤버 제거
  - `BeginPlay`에서 `DataCenter->GetConfig<UExRunnerConfig>()` 조회 후 `TWeakObjectPtr` 캐싱
- [x] `UExObstacleManager` 등 — `UExObstacleSpawnConfig*` 직접 참조 → DataCenter 조회로 교체
- [x] **[CoreRedirects 작성]** `DefaultEngine.ini`에 리디렉트 추가
  ```ini
  [CoreRedirects]
  +ClassRedirects=(OldName="/Script/ExRunnerPlayRuntime.ExCurveConfig",NewName="/Script/ExRunnerPlayRuntime.ExRunnerConfig")
  +ClassRedirects=(OldName="/Script/ExRunnerPlayRuntime.ExObstacleSpawnConfig",NewName="/Script/ExRunnerPlayRuntime.ExRunnerConfig")
  ```
- [x] **[기존 DA 삭제 또는 대체]** 에디터에서 `DA_ExCurveConfig`, `DA_ExObstacleSpawnConfig` → 새 `DA_ExConfig_Runner` 생성 및 값 이전
- [x] **[일괄 리세이브]** Resave Packages 실행
- [x] 빌드 검증
- [x] 기능 검증 (커브 경로, 장애물 배치 기존과 동일)

---

### Phase 3 — Definition 마이그레이션

#### 3-A. `UExObstacleDefinition` — ExCore → ExRunnerPlay 이동 + Base 교체

- [x] **[이동]** `ExObstacleDefinition.h`를 `ExRunnerPlay/Source/ExRunnerPlayRuntime/Data/`로 이동
  - `EXCORERUNTIME_API` → `EXRUNNERPLAYRUNTIME_API` 변경
  - include 경로 정리
- [x] **[상속 변경]** `UDataAsset` → `UExDefinitionDataAsset`
  - `DefinitionTag` — 부모에서 상속됨 (추가 불필요)
- [x] **[IsDataValid]** `Super::IsDataValid()` 호출 + `ObstacleClass` nullptr 검증 추가
- [x] **[CoreRedirects 작성]**
  ```ini
  [CoreRedirects]
  +ClassRedirects=(OldName="/Script/ExCoreRuntime.ExObstacleDefinition",NewName="/Script/ExRunnerPlayRuntime.ExObstacleDefinition")
  ```
- [x] **[일괄 리세이브]** Resave Packages 실행
- [x] 빌드 검증
- [x] 기능 검증

#### 3-B. `UExItemDefinition` — Base 교체 + `ItemTag` → `DefinitionTag` 통합

- [x] **[상속 변경]** `UPrimaryDataAsset` → `UExDefinitionDataAsset`
- [x] **[멤버 변경]** 기존 `ItemTag` 멤버 제거 — 부모의 `DefinitionTag`로 통합
- [x] **[IsDataValid]** `PickupActorClass` nullptr 검증 추가
- [x] **[참조 교체]** 코드 내 `ItemDef->ItemTag` → `ItemDef->DefinitionTag` 전체 교체
- [x] **[CoreRedirects 작성]**
  ```ini
  [CoreRedirects]
  +PropertyRedirects=(OldName="/Script/ExCoreRuntime.ExItemDefinition.ItemTag",NewName="DefinitionTag")
  ```
- [x] **[일괄 리세이브]** Resave Packages 실행
- [x] 빌드 검증
- [x] 기능 검증 (아이템 스폰/획득)

---

### Phase 4 — Preset 마이그레이션

#### 4-A. `UExRunnerRuleConfig` — Preset 계열 전환

- [x] **[상속 변경]** `UDataAsset` → `UExPresetDataAsset`
  - `PresetTag` — 부모에서 상속됨
- [x] **[IsDataValid]** `Rules` 배열 비어있으면 경고 추가 / nullptr 룰 에러 추가
- [x] **[에디터]** 기존 DA_ExRule 에셌 `PresetTag` 값 설정 완료
  - `DA_ExRule` → PresetTag: `Ex.Runner.Rule`
- [x] **[GFAction 연결]** `Add Ex Data > Preset Assets`에 모드별 DA 등록 완료
- [x] **[참조 교체]** `UExRunnerRuleManagerComponent` → DataCenter 조회로 교체
- [x] 빌드 검증 및 DataCenter 동적 등록 검증 완료
- [x] 기능 검증 (타이머, FallDeath, DistanceGoal 룰 동작)

#### 4-B. `UExRunnerItemSpawnTable` — Preset 계열 전환

- [x] **[상속 변경]** `UDataAsset` → `UExPresetDataAsset`
- [x] **[IsDataValid]** `CoinEntries` 및 `BuffEntries`의 유효성(null 확인/가중치 검사) 추가
- [x] **[GFAction 연결]** `Add Ex Data > Preset Assets`에 등록 *(이 단계에서 수행)*
- [x] **[참조 교체]** `UExRunnerItemManager` → DataCenter 조회로 교체
- [x] 빌드 검증
- [x] 기능 검증 (코인/버프 스폰)

---

### Phase 5 — GameFeatureData 액션 등록

- [ ] `ExRunnerPlay.uasset` GameFeatureData 에디터 열기
- [ ] **Actions** 탭에 `Add Ex Data` 액션 추가
- [ ] `Config Asset` 슬롯: `DA_ExConfig_Runner` 지정
- [ ] `Definition Assets` 슬롯: `DA_ExObstacle_Gap`, `DA_ExObstacle_WallRun`, `DA_ExObstacle_Climb`, `DA_ExObstacle_Slide`, `DA_ExItem_Coin`, `DA_ExItem_SpeedBoost` 등록
- [ ] `Preset Assets` 슬롯: 모드별 RuleConfig DA, ItemSpawnTable DA 등록
- [ ] PIE 테스트 — GameFeature 활성화 시 DataCenter 등록 로그 확인
- [ ] PIE 테스트 — GameFeature 비활성화 시 DataCenter 해제 로그 확인

---

### Phase 6 — 최종 검증

- [ ] 전체 빌드 에러 없음
- [ ] PIE 실행 — 러너 게임플레이 전체 기능 검증
  - [ ] 커브 경로 생성
  - [ ] 장애물 스폰 (Gap, WallRun, Climb, Slide)
  - [ ] 코인/버프 아이템 스폰
  - [ ] 룰 시스템 (타이머, FallDeath, DistanceGoal)
- [ ] DataCenter 로그 확인 (등록/해제 메시지 정상)
- [ ] 기존 에셋(.uasset) 참조 끊김 없음 확인
- [ ] 기존 `UExCurveConfig`, `UExObstacleSpawnConfig` 클래스 파일 삭제

---

## 📝 마이그레이션 중 발견된 추가 항목

> 작업 진행 중 발견되는 내용을 여기에 추가 기록합니다.

| 발견일 | Phase | 항목 | 내용 |
|---|---|---|---|
| - | - | - | - |
