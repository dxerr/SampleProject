# ExFrameWork: 통합 데이터 센터 시스템 설계서

> **버전:** v1.2  
> **대상 엔진:** Unreal Engine 5  
> **프로젝트:** ExFrameWork  
> **작성일:** 2026-04-20  
> **최종 수정:** 2026-04-20 (v1.2: 복합키 저장소 전환, 캐싱 TWeakObjectPtr 강제, Core/Feature 구조 개선, 마이그레이션 안전성 수정 반영)  
> **설계 방향:** GameInstance Subsystem 기반 3-Base DataAsset 체계  
> **의존 문서:**  
> - ExFrameWork_Guidelines.md  
> - ExFrameWork_Item_System_Architecture.md  
> - ExRunner_System_Architecture.md  

---

## 1. 문제 정의

### 1.1 현재 상황

프로젝트 내 데이터가 11개 이상의 독립 DataAsset 클래스로 분산되어 있다.
각 DataAsset은 서로 다른 모듈(ExFrameWork 루트, ExCore, ExRunnerPlay)에 배치되어 있으며,
참조 관계가 GameMode를 중심으로 수동 연결되어 있다.

**핵심 고통점:**

- 데이터 수정 시 여러 DA를 각각 찾아 열어야 하는 번거로움
- 새로운 데이터를 추가할 때 어디에, 어떤 형태로 만들어야 하는지 판단이 어려움
- 전체 데이터 현황을 한눈에 파악할 수 없음
- 삭제·이동 시 참조 누락 위험

### 1.2 해결 목표

1. **단일 창구 원칙**: 어떤 클래스에서든 하나의 인터페이스를 통해 모든 데이터에 접근
2. **데이터 성격별 분류 체계**: 새 데이터 생성 시 "어떤 Base를 상속받을지" 명확한 판단 기준 제공
3. **편집 효율**: 설정형 데이터는 DA 하나에 통합하여 열어야 하는 파일 수를 최소화
4. **전체 파악 용이**: 에디터 Content Browser에서 PrimaryAssetType 필터로 전체 데이터를 3그룹으로 즉시 조망
5. **ExCore/Feature 분리 원칙 준수**: Core는 Feature를 절대 참조하지 않는다
6. **정적 데이터 전용**: DataCenter는 읽기 전용 정적(Static) 데이터만 관리한다. 런타임 동적 상태(점수, 세션 데이터 등)는 별도 Subsystem 또는 GameState에서 관리한다 (SRP 준수)

---

## 2. 데이터 유형 분류 체계 (3-Base 모델)

### 2.1 분류 기준

프로젝트의 모든 DataAsset을 데이터의 **성격**에 따라 3가지 Base로 분류한다.
새 데이터를 만들 때는 아래 판단 트리를 따른다.

```
"이 데이터는 프로젝트에 인스턴스가 몇 개 필요한가?"
│
├─ "딱 하나. 전역 수치/설정이다" ────────→ Config (설정형)
│
└─ "여러 개. 종류만큼 필요하다"
    │
    ├─ "개체 자체를 정의한다 (이것은 무엇인가)" ──→ Definition (정의형)
    │
    └─ "정의들의 조합/룰이다 (이번 판은 이렇게)" ──→ Preset (프리셋형)
```

### 2.2 3-Base 상세 정의

#### A. Config (설정형) — `UExConfigDataAsset`

| 항목 | 내용 |
|---|---|
| **성격** | 전역 수치 튜닝. INI 파일과 유사한 역할 |
| **인스턴스 수** | 모듈당 1개 (싱글톤) |
| **PrimaryAssetType** | `ExConfig` |
| **판단 기준** | "이 값을 바꾸면 게임 전체에 영향을 미치는가?" → Yes → Config |
| **현재 해당 데이터** | ExGameModeDataSet, ExCurveConfig, ExObstacleSpawnConfig |
| **핵심 전략** | **컴포지트 구조**: 기존 독립 DA 클래스들을 USTRUCT로 변환 후, 하나의 마스터 Config DA에 멤버로 통합 |

**컴포지트 Config 전략 상세:**

기존에 각각 독립 UDataAsset 클래스였던 설정 데이터를 USTRUCT(구조체)로 변환한다.
변환된 구조체들을 하나의 마스터 Config DataAsset 클래스의 멤버 프로퍼티로 선언한다.
에디터의 Details 패널에서 UPROPERTY의 Category 문자열로 자동 그룹핑되므로,
DA 하나를 열면 "1. GameMode", "2. Curve", "3. ObstacleSpawn" 등의 접힌 카테고리 섹션이 나타난다.

이를 통해 설정 수정 시 열어야 하는 DA 수가 기존 4~5개에서 **모듈당 1개**로 감소한다.

**USTRUCT vs Instanced UObject 선택 근거:**

Config 내부 섹션은 USTRUCT를 기본으로 한다. 현재 Config의 성격은 수치 튜닝이지 알고리즘 교체가 아니다.
알고리즘 교체가 필요한 경우는 이미 Strategy Pattern(ExObstacleSpawnStrategy 계열)으로 처리하고 있다.
USTRUCT는 가볍고, 에디터 Details 패널에서 별도 DA를 열지 않고 인라인으로 바로 편집할 수 있는 장점이 있다.
단, 향후 Config 내 특정 섹션에 다형성이 필요해지면 해당 섹션만 Instanced UObject로 전환할 수 있다.

**모듈별 Config DA 배치:**

| Config DA | 배치 모듈 | 포함 내용 |
|---|---|---|
| `DA_ExConfig_Core` | ExCore | 범용 메타 설정 (UI 글로벌 테마, 볼륨 리미트 등). 특정 모드나 장르를 유추할 수 있는 값은 절대 포함하지 않는다 |
| `DA_ExConfig_Runner` | ExRunnerPlay | 게임모드 수치, 커브 경로, 장애물 스폰, 아이템 스폰 수치 |

> **[중요]** ExRunnerPlay의 Config DA가 ExCore의 Config 값을 참조해야 하는 경우,
> DataCenter Subsystem을 통해 간접 접근한다. 직접 include 하지 않는다.

#### B. Definition (정의형) — `UExDefinitionDataAsset`

| 항목 | 내용 |
|---|---|
| **성격** | 개체 메타데이터. "이것은 무엇인가"를 기술 |
| **인스턴스 수** | 종류만큼 N개 |
| **PrimaryAssetType** | `ExDefinition` |
| **판단 기준** | "이 데이터는 고유한 비주얼, 이펙트, 속성을 가진 독립 개체를 정의하는가?" → Yes → Definition |
| **현재 해당 데이터** | ExItemDefinition, ExObstacleDefinition |
| **핵심 전략** | 개별 DA는 유지하되, **매니페스트 DA**를 두어 전체 목록을 한곳에서 관리 |

**매니페스트 전략 상세:**

Definition은 Config와 달리 하나의 DA로 합칠 수 없다.
각 아이템/장애물이 고유한 서브오브젝트(Instanced 이펙트, Soft Reference 등)를 가지고 있어
하나의 DA에 모두 넣으면 오히려 편집이 복잡해지기 때문이다.

대신, 모든 Definition DA를 참조 목록으로 모아놓는 **매니페스트 DA**를 만든다.
매니페스트 DA는 TArray<TObjectPtr<해당Definition타입>> 형태의 배열 프로퍼티를 가진다.
에디터에서 매니페스트 DA 하나를 열면 등록된 모든 정의를 한눈에 볼 수 있고,
각 항목을 클릭하면 해당 개별 DA로 즉시 이동할 수 있다.

**매니페스트 하드 참조 정책:**

현재 프로젝트의 Definition 수는 아이템 3~5개, 장애물 4개 수준이며,
모바일 러너 게임 특성상 한 세션에 등록된 모든 개체를 실제로 사용하므로
전부 메모리에 로딩되어야 한다. 따라서 초기에는 TObjectPtr(하드 참조)로 구현한다.

> **[확장 방침]** Definition이 50개 이상으로 확장될 경우,
> 매니페스트 배열을 TSoftObjectPtr 또는 FPrimaryAssetId 기반으로 전환하고
> 필요 시점에 비동기 로드(Async Load)하는 구조로 마이그레이션한다.

**작업 플로우:**

1. 새 개체(예: 신규 아이템) 추가 시 → 개별 Definition DA 생성 → 내부 속성 편집
2. 매니페스트 DA를 열어 배열에 새 DA를 추가 (드래그 또는 선택)
3. DataCenter Subsystem이 매니페스트를 읽어 자동 등록
4. 삭제 시 → 매니페스트에서 제거 → DA 파일 삭제

**매니페스트 DA 배치:**

| 매니페스트 DA | 배치 모듈 | 포함 내용 |
|---|---|---|
| `DA_ExManifest_Runner` | ExRunnerPlay | 장애물 정의 목록, 아이템 정의 목록 |

> **[참고]** ExCore에 정의되는 범용 Definition이 늘어나면 `DA_ExManifest_Core` 매니페스트를 별도 생성한다.

#### C. Preset (프리셋형) — `UExPresetDataAsset`

| 항목 | 내용 |
|---|---|
| **성격** | Definition들의 조합과 룰. "이번 판은 이렇게 플레이한다" |
| **인스턴스 수** | 모드/난이도별 M개 |
| **PrimaryAssetType** | `ExPreset` |
| **판단 기준** | "이 데이터는 다른 Definition들을 조합하고 조건/확률/순서를 정의하는가?" → Yes → Preset |
| **현재 해당 데이터** | ExRunnerRuleConfig, ExRunnerItemSpawnTable |
| **핵심 전략** | 모드·난이도별로 자연스럽게 여러 인스턴스가 존재. Definition 참조 + 조건 수치를 함께 보유 |

**Config와 Preset의 결정적 차이:**

- Config는 "속도 600, 확률 0.5" 같은 **수치 자체** (프로젝트에 하나)
- Preset은 "이 Definition들을 이 조건으로 조합한다"는 **구성(Composition)** (모드별 여러 개)

예시: 아이템 스폰 테이블은 "코인 엔트리 목록 + 확률 + 패턴 조건"을 조합하므로 Preset이다.
게임 룰 설정은 "이 룰 Definition들을 이 모드에서 활성화한다"이므로 Preset이다.

**Preset DA 예시:**

| Preset DA | 용도 |
|---|---|
| `DA_ExPreset_EndlessMode` | 엔들리스 모드의 룰 조합 + 아이템 배치 전략 |
| `DA_ExPreset_TimerMode` | 타이머 모드의 룰 조합 + 아이템 배치 전략 |
| `DA_ExPreset_EasyDifficulty` | 이지 난이도의 스폰 확률/간격 조정 |

---

### 2.3 분류 체계 종합 다이어그램

```
UPrimaryDataAsset
│
├── UExConfigDataAsset (Abstract, ExCore)
│    │  PrimaryAssetType: "ExConfig"
│    │  특성: 싱글톤, USTRUCT 멤버로 설정 통합
│    │
│    ├── UExCoreConfigAsset (ExCore)
│    │    └ DA_ExConfig_Core (인스턴스 1개)
│    │
│    └── UExRunnerConfigAsset (ExRunnerPlay)
│         └ DA_ExConfig_Runner (인스턴스 1개)
│
├── UExDefinitionDataAsset (Abstract, ExCore)
│    │  PrimaryAssetType: "ExDefinition"
│    │  특성: N개 인스턴스, 매니페스트로 목록 관리
│    │
│    ├── UExItemDefinition (ExCore)
│    │    ├ DA_ExItem_Coin
│    │    ├ DA_ExItem_SpeedBoost
│    │    └ DA_ExItem_Magnet ...
│    │
│    └── UExObstacleDefinition (ExRunnerPlay)      ← (러너 장르 전용이므로 구체 클래스는 Feature에 위치)
│         ├ DA_ExObstacle_Gap
│         ├ DA_ExObstacle_Climb
│         └ DA_ExObstacle_Slide ...
│
└── UExPresetDataAsset (Abstract, ExCore)
     │  PrimaryAssetType: "ExPreset"
     │  특성: M개 인스턴스, 모드/난이도별 조합
     │
     ├── UExRunnerRulePreset (ExRunnerPlay)
     │    ├ DA_ExPreset_EndlessMode
     │    └ DA_ExPreset_TimerMode
     │
     └── UExRunnerSpawnPreset (ExRunnerPlay)
          ├ DA_ExPreset_EasyDifficulty
          └ DA_ExPreset_HardDifficulty
```

---

## 3. DataCenter Subsystem 설계

### 3.1 개요

`UExDataCenterSubsystem`은 `UGameInstanceSubsystem`을 상속받는 중앙 데이터 관리자이다.
ExCore 모듈에 배치하며, 게임 인스턴스 생존 기간 동안 유지된다.
**읽기 전용 정적 데이터 전용**이며, 런타임 동적 상태는 취급하지 않는다.

**선택 근거:**

- 레벨 전환에도 유지되어 Config 데이터 재로딩 불필요
- 엔진이 생명주기를 자동 관리 (수동 싱글톤 관리 불필요)
- 프로젝트 내 기존 패턴(`UExMusicManagerSubsystem`)과 일관성 확보
- Blueprint에서 `GetGameInstanceSubsystem` 노드로 쉽게 접근 가능

### 3.2 핵심 책임

| 책임 | 설명 |
|---|---|
| **Config 관리** | 클래스 타입을 키로 1:1 매핑. 타입 지정만으로 해당 Config 즉시 반환 |
| **Definition 관리** | GameplayTag를 키로 1:N 매핑. 태그 기반 검색 및 전체 목록 조회 |
| **Preset 관리** | Definition과 동일한 태그 기반 관리. 모드/난이도 태그로 필터링 |
| **GameFeature 연동** | Feature 활성화 시 해당 Feature의 DA들을 자동 등록, 비활성화 시 해제 |

### 3.3 내부 저장소 구조

DataCenter는 내부적으로 3개의 저장소를 운용한다.
현재 규모(DA 20개 미만)에서는 자체 TMap으로 직접 관리하는 것이 AssetManager 래핑보다 단순하고 직관적이다.

> **[확장 방침]** DA가 50개 이상이 되거나 비동기 로딩이 필수가 되는 시점에
> 내부 저장소를 UAssetManager 기반으로 교체한다.
> 이때 외부 API 시그니처는 변경하지 않는다 (§3.5 API 안정성 원칙 참조).

```
UExDataCenterSubsystem
│
├── ConfigMap (TMap)
│    Key: UClass* (Config DA의 구체 클래스)
│    Value: UExConfigDataAsset*
│    관계: 1:1 (클래스당 인스턴스 하나)
│    접근: GetConfig<T>() → 타입만 지정하면 즉시 반환
│
├── DefinitionMap (TMap)
│    Key: UClass* (서브클래스 타입)
│    Value: TMap<FGameplayTag, UExDefinitionDataAsset*> (태그별 단일 매핑)
│    관계: 동일 클래스 내 태그 유일성 강제
│    접근: FindDefinition<T>(Tag), GetAllDefinitions<T>()
│
└── PresetMap (TMap)
     Key: UClass* (서브클래스 타입)
     Value: TMap<FGameplayTag, UExPresetDataAsset*> (모드/난이도 태그 매핑)
     관계: 클래스별로 프리셋 공간을 분리하여 동일 모드 태그 공유 시 덮어쓰기 충돌 방지
     접근: GetPreset<T>(ModeTag)
```

### 3.4 접근 패턴 요약

| 상황 | 호출 방식 | 반환 |
|---|---|---|
| 커브 반경 값이 필요할 때 | `DataCenter->GetConfig<UExRunnerConfigAsset>()` → `.Curve.FixedCurveRadius` | 단일 float |
| 코인 아이템 정의가 필요할 때 | `DataCenter->FindDefinition<UExItemDefinition>(Ex.Item.Coin 태그)` | 단일 DA 포인터 |
| 모든 장애물 정의 목록 | `DataCenter->GetAllDefinitions<UExObstacleDefinition>()` | TArray |
| 현재 모드의 룰 프리셋 | `DataCenter->GetPreset<UExRunnerRulePreset>(Ex.Mode.Endless 태그)` | 단일 DA 포인터 |

### 3.5 API 안정성 원칙

DataCenter의 공개 API(GetConfig, FindDefinition, GetAllDefinitions, GetPreset)는
**안정 인터페이스**로 취급한다. 내부 저장소 구현(TMap, AssetManager 등)이 변경되더라도
이 외부 API 시그니처는 유지한다. 호출하는 쪽(GameMode, UI, Component 등)은
DataCenter 내부가 어떻게 바뀌었는지 알 필요가 없으며, 코드를 수정할 필요가 없다.

### 3.6 데이터 접근 캐싱 원칙

**DataCenter API 호출은 초기화 시점에 1회 수행하고, 결과를 로컬 포인터에 캐싱하여 사용한다.**

DataCenter의 TMap 검색 자체는 해싱 기반으로 단일 호출이 가볍지만,
Tick/Update 등 매 프레임 호출 시 Subsystem 접근 간접 참조 비용이 누적된다.
따라서 다음 원칙을 반드시 준수한다:

- **허용**: BeginPlay, OnExperienceLoaded, InitializeComponent 등 초기화 시점에서 호출 후 멤버 변수에 캐싱
- **금지**: Tick, Update, 매 프레임 콜백 내에서 DataCenter API를 직접 호출
- **캐싱 타입 보장**: 캐싱 변수는 **반드시 `TWeakObjectPtr<T>`만을 허용**하며, Raw Pointer 캐싱은 엄격히 금지한다. GameFeature 비활성화 시 캐시가 해제(Stale)될 수 있기 때문이다.
- (선택) `OnDataCenterUpdated` 이벤트 수신 시 유효성 잃은 캐시를 재조회하거나 무효화하는 흐름 구축.

### 3.7 에러 알림 정책

DataCenter의 GetConfig, FindDefinition 등이 nullptr을 반환하는 상황(데이터 미등록)은
치명적 설정 오류이므로 즉각 인지할 수 있어야 한다.

**알림 수단:**

- `ensureMsgf`를 사용하여 에디터 팝업 경고 + 콜스택을 표시한다. 에디터가 크래시하지는 않는다
- 추가로 `GEngine->AddOnScreenDebugMessage`로 화면에 붉은색 텍스트를 표시하여 1인 개발 환경에서 즉각 인지할 수 있게 한다. 
  - 단, 라이브 서버 및 쓰레기 출력 방지를 위해 **반드시 `#if !UE_BUILD_SHIPPING`과 `IsRunningDedicatedServer()` 체크**로 분기 가드를 작성한다.

> **[주의]** `UE_LOG(Fatal)`은 에디터를 즉시 크래시시키므로 사용하지 않는다.
> 가이드라인 §1.7의 검증 규칙(ensure/check 선택 기준)을 따른다.

### 3.8 GameFeature 연동 메커니즘

GameFeature 플러그인이 활성화될 때 해당 Feature의 DataAsset들을 DataCenter에 자동 등록하는 흐름:

```
GameFeature 활성화 (OnGameFeatureActivating)
│
├── 1. GameFeatureAction_AddExData (커스텀 액션)이 발동
│
├── 2. 액션이 지정된 Config DA, Definition 매니페스트 DA, Preset DA 목록을 읽음
│
├── 3. DataCenter의 Register 함수들을 순차 호출
│
└── 4. 등록 완료 → OnDataCenterUpdated 델리게이트 브로드캐스트
       (관심 있는 시스템이 구독하여 갱신 처리)

GameFeature 비활성화 (OnGameFeatureDeactivating)
│
└── DataCenter->UnregisterByFeature(FeatureName) 호출
     → 해당 Feature가 등록한 모든 데이터 일괄 해제
```

> **[핵심]** 이 메커니즘을 통해 ExCore의 DataCenter는 ExRunnerPlay의 존재를 알 필요가 없다.
> Feature 쪽에서 자신의 데이터를 DataCenter에 "밀어넣는(Push)" 방식이므로 의존성 방향이 올바르다.

---

## 4. 데이터 유효성 검증 (Data Validation)

### 4.1 개요

수많은 DA가 얽히는 구조이므로, 기획 실수를 에디터 단에서 사전에 차단하는 장치가 필수적이다.
3개 Base 클래스 모두에 `IsDataValid(FDataValidationContext& Context)` 오버라이드를 추가한다.

### 4.2 검증 체계 — 2단계 방어

**1단계 (에디터 수준):** UPROPERTY의 `meta = (ClampMin, ClampMax)` 등으로 입력 범위를 제한하여
애초에 잘못된 값을 기입할 수 없게 한다.

**2단계 (IsDataValid):** 저장(Save) 시점 또는 패키징 시점에 논리적 정합성을 검증한다.

### 4.3 Base별 검증 항목

| Base | 검증 항목 |
|---|---|
| **Config** | 필수 수치 범위 검증 (확률 0~1, 거리 양수 등). USTRUCT 멤버가 비정상적 기본값인지 확인 |
| **Definition** | 식별 태그(DefinitionTag)가 비어있지 않은지. 필수 클래스 참조(PickupActorClass 등)가 nullptr이 아닌지. Instanced Effect가 할당되었는지 |
| **Preset** | Definition 배열에 nullptr 항목이 포함되지 않았는지. 가중치/확률 합계가 유효 범위인지. 참조하는 Definition DA가 실제 존재하는지 |

### 4.4 구현 위치

IsDataValid 로직은 **C++ 단일 위치**에서 관리한다.
BlueprintNativeEvent로 BP에 검증 확장 포인트를 여는 방식은 현재 1인 개발 환경에서 불필요하다.
검증 로직이 C++과 BP 두 곳에 분산되면 관리 포인트가 늘어나 오히려 역효과이다.
팀 확장 시 BP 검증 확장 필요성을 재검토한다.

---

## 5. 에디터 검색성 확보 방안

### 5.1 PrimaryAssetType 기반 필터링

3개의 Base 클래스가 각각 `GetPrimaryAssetId()`를 오버라이드하여 고유 PrimaryAssetType을 반환한다.
`DefaultEngine.ini`의 `PrimaryAssetTypesToScan`에 3개 타입을 등록하면,
에디터의 **Asset Audit** 창에서 타입별로 전체 데이터 에셋을 트리 형태로 조망할 수 있다.

| PrimaryAssetType | 대상 Base | Content Browser 필터 결과 |
|---|---|---|
| `ExConfig` | UExConfigDataAsset 계열 | DA_ExConfig_Core, DA_ExConfig_Runner |
| `ExDefinition` | UExDefinitionDataAsset 계열 | DA_ExItem_Coin, DA_ExObstacle_Gap 등 전체 |
| `ExPreset` | UExPresetDataAsset 계열 | DA_ExPreset_EndlessMode 등 전체 |

### 5.2 AssetRegistrySearchable 메타 활용

Definition 계열 DA의 식별 태그 프로퍼티에 `AssetRegistrySearchable` 메타를 부여하면,
Content Browser의 고급 필터에서 프로퍼티 값으로 검색할 수 있다.

예: `DefinitionTag == Ex.Item.Coin`으로 특정 아이템만 필터링 가능.

### 5.3 에셋 명명 규칙

ExFrameWork_Guidelines의 §1.2 에셋 명명 규칙을 확장한다:

| 유형 | 접두사 패턴 | 예시 |
|---|---|---|
| Config DA | `DA_ExConfig_[모듈]` | `DA_ExConfig_Core`, `DA_ExConfig_Runner` |
| Definition DA | `DA_Ex[타입]_[이름]` | `DA_ExItem_Coin`, `DA_ExObstacle_Gap` |
| Definition 매니페스트 | `DA_ExManifest_[모듈]` | `DA_ExManifest_Runner` |
| Preset DA | `DA_ExPreset_[용도]` | `DA_ExPreset_EndlessMode` |

### 5.4 장기 확장: 커스텀 에디터 유틸리티 (선택사항)

데이터가 50개 이상으로 늘어나면, EditorUtilityWidget 기반의 "Ex Data Browser"를 제작하여
Config/Definition/Preset을 한 화면에서 트리 형태로 보고 바로 편집할 수 있게 한다.
이는 즉시 필수는 아니며, 데이터 볼륨에 따라 판단한다.

---

## 6. 폴더 구조

### 6.1 소스 코드 배치

```
ExCore/Source/ExCoreRuntime/
├── Data/
│    ├── Base/
│    │    ├── ExConfigDataAsset.h        ← Config Base (Abstract)
│    │    ├── ExDefinitionDataAsset.h    ← Definition Base (Abstract)
│    │    └── ExPresetDataAsset.h        ← Preset Base (Abstract)
│    │
│    ├── ExFeatureAssetManifest.h        ← 기존 에셋 매니페스트 (유지)
│    ├── ExItemDefinition.h             ← Definition 구현 (범용 아이템이므로 Core)
│
├── Subsystems/
│    └── ExDataCenterSubsystem.h/.cpp   ← DataCenter Subsystem
│
└── Actions/
     └── GameFeatureAction_AddExData.h  ← GameFeature 데이터 등록 액션

ExRunnerPlay/Source/ExRunnerPlayRuntime/
├── Data/
│    ├── Definitions/
│    │    └── ExObstacleDefinition.h     ← Definition 구현 (러너 전용 장애물이므로 Feature로 이동)
│    │
│    ├── Config/
│    │    ├── ExRunnerConfigAsset.h      ← Runner Config DA 클래스
│    │    ├── FExGameModeConfig.h        ← USTRUCT (기존 ExGameModeDataSet 변환)
│    │    ├── FExCurvePathConfig.h       ← USTRUCT (기존 ExCurveConfig 변환)
│    │    └── FExObstacleSpawnConfig.h   ← USTRUCT (기존 변환)
│    │
│    ├── Presets/
│    │    ├── ExRunnerRulePreset.h       ← Preset 구현 (기존 ExRunnerRuleConfig 리네임)
│    │    └── ExRunnerSpawnPreset.h      ← Preset 구현 (기존 ExRunnerItemSpawnTable 리네임)
│    │
│    └── (기존 Strategy 파일들 유지)
```

### 6.2 Content 에셋 배치

```
ExRunnerPlay/Content/Data/
├── Config/
│    └── DA_ExConfig_Runner.uasset      ← Runner 설정 통합 DA (1개)
│
├── Definitions/
│    ├── Items/
│    │    ├── DA_ExItem_Coin.uasset
│    │    ├── DA_ExItem_SpeedBoost.uasset
│    │    └── DA_ExItem_Magnet.uasset
│    │
│    ├── Obstacles/
│    │    ├── DA_ExObstacle_Gap.uasset
│    │    ├── DA_ExObstacle_Climb.uasset
│    │    ├── DA_ExObstacle_Slide.uasset
│    │    └── DA_ExObstacle_WallRun.uasset
│    │
│    └── DA_ExManifest_Runner.uasset    ← 정의 매니페스트 (1개)
│
└── Presets/
     ├── DA_ExPreset_EndlessMode.uasset
     └── DA_ExPreset_TimerMode.uasset
```

---

## 7. 마이그레이션 전략

### 7.1 단계별 진행

기존 시스템을 한 번에 전환하지 않고, 3단계에 걸쳐 점진적으로 마이그레이션한다.

**1단계: 인프라 구축 (Base 클래스 + DataCenter)**

- 3개 Base 클래스 생성 (Abstract, ExCore에 배치)
- 각 Base에 IsDataValid() 오버라이드 포함
- UExDataCenterSubsystem 생성 (ExCore에 배치)
- GameFeatureAction_AddExData 생성 (ExCore에 배치)
- 전용 로그 카테고리 `LogExDataCenter` 선언
- 기존 코드는 변경하지 않음. 새 인프라만 추가

**2단계: Config 통합 (가장 효과가 큰 영역 우선)**

- ExGameModeDataSet → FExGameModeConfig (USTRUCT 변환)
- ExCurveConfig → FExCurvePathConfig (USTRUCT 변환)
- ExObstacleSpawnConfig → FExObstacleSpawnConfig (USTRUCT 변환)
- UExRunnerConfigAsset 생성, 위 3개 구조체를 멤버로 통합
- 기존 참조 코드를 DataCenter 경유로 전환
- 각 전환 대상에 대해 초기화 시점 캐싱 패턴 적용

**3단계: Definition/Preset 정리**

- ExItemDefinition, ExObstacleDefinition의 Base를 UExDefinitionDataAsset으로 변경
- ExRunnerRuleConfig, ExRunnerItemSpawnTable의 Base를 UExPresetDataAsset으로 변경
- 매니페스트 DA 생성 및 등록 연동
- GameMode의 직접 DA 참조를 DataCenter 경유로 전환

### 7.2 호환성 보장 및 에셋 리세이브

단순히 UDataAsset을 USTRUCT로 변환하고 클래스명을 바꾼다고 참조 호환성이 자동으로 유지되지는 않는다. 기존 레퍼런스가 끊어지는 것을 막기 위해 다음 플로우를 필수적으로 따른다.
1. `DefaultEngine.ini`에 `CoreRedirects` (ClassRedirects 등)를 작성하여 엔진에 경로 변경을 알린다.
2. 에디터를 구동 후, 전환 대상이 된 구형 에셋들과 이를 참조하고 있는 시스템을 모두 열어 정상 연결 여부를 확인한다.
3. 관련 폴더 내 에셋들의 전체 일괄 리세이브(Resave Packages)를 실행하여 리다이렉션을 물리 에셋 파일에 확정 짓는다.

---

## 8. 설계 원칙 및 제약 사항

### 8.1 Core/Feature 의존성 규칙

- 3개 Base 클래스와 DataCenter Subsystem은 반드시 **ExCore**에 배치
- ExCore는 ExRunnerPlay를 절대 참조하지 않음
- ExRunnerPlay는 ExCore의 Base를 상속받아 구체 Config/Preset 클래스를 구현
- Feature → DataCenter에 데이터를 Push하는 방향 (Core가 Feature를 Pull하지 않음)

### 8.2 정적 데이터 전용 원칙 (SRP)

DataCenter는 읽기 전용 정적 데이터(Config, Definition, Preset)만 관리한다.
점수, 세션 정보, 플레이 통계 등 런타임 동적 상태는 별도의 Subsystem(예: UExSessionSubsystem) 
또는 GameState에서 관리한다. 데이터 오염과 책임 혼재를 방지한다.

### 8.3 네이밍 규칙

| 대상 | 접두사 | 예시 |
|---|---|---|
| Config Base | `UExConfigDataAsset` | — |
| Config 통합 USTRUCT | `FEx[영역]Config` | `FExCurvePathConfig` |
| Config DA 클래스 | `UEx[모듈]ConfigAsset` | `UExRunnerConfigAsset` |
| Definition Base | `UExDefinitionDataAsset` | — |
| Preset Base | `UExPresetDataAsset` | — |
| DataCenter | `UExDataCenterSubsystem` | — |
| GF 액션 | `GameFeatureAction_AddExData` | — |

### 8.4 로그 카테고리

프로젝트 가이드라인 §1.7에 따라 전용 로그 카테고리를 사용한다:
`DECLARE_LOG_CATEGORY_EXTERN(LogExDataCenter, Log, All);`

### 8.5 Assert 및 에러 처리 규칙

- **Config 단건 접근 에러**: 등록되지 않은 경우 `ensureMsgf` 경고 + 디버그 화면 메시지. (DataCenter API 오류는 Fatal 금지)
- **Definition 매니페스트에 nullptr 검출 시 심각도 구분**:
  - 에디터 개발 시점 (`IsDataValid` 검증기): **Error**를 띄워 애초에 파일 저장을 차단.
  - 런타임 등록 시점 (GameFeature 로딩): 예기치 못한 게임 파괴를 막기 위해 **Warning** 모드로 로깅 후 해당 항목만 무시하고 Skip.
- **환경 가드**: 화면 디버그 메시지 기능은 `#if !UE_BUILD_SHIPPING` 내부에서만 수행.

### 8.6 데이터 접근 캐싱 규칙

- DataCenter API 호출은 초기화 시점(BeginPlay 등)에 1회만 수행
- 결과를 로컬 멤버 변수에 캐싱하여 이후 프레임에서 재사용
- Tick/Update 내에서 DataCenter API 직접 호출 금지

---

## 9. 기대 효과 요약

| 지표 | 현재 | 개선 후 |
|---|---|---|
| 설정 수정 시 열어야 하는 DA 수 | 4~5개 | **1개** (Config DA) |
| 전체 데이터 파악 방법 | 폴더 탐색 | **Asset Audit에서 3개 타입 필터** |
| 새 데이터 생성 시 판단 | "어디에? 어떤 형태로?" | **판단 트리 1개 질문으로 결정** |
| 데이터 접근 코드 | GameMode에서 직접 참조 + 캐스팅 | **DataCenter 단일 API** |
| GameFeature 데이터 관리 | 수동 참조 연결 | **자동 등록/해제** |
| 기획 실수 감지 | 런타임 크래시 | **저장 시점 IsDataValid 검증** |
| 데이터 미등록 감지 | 로그창 확인 필요 | **화면 즉시 알림 (ensureMsgf + OnScreenDebug)** |

---

## 10. 미결 사항 및 확장 방침

### 10.1 확정된 방침 (피드백 반영 완료)

| 항목 | 결정 | 근거 |
|---|---|---|
| 런타임 상태 관리 | DataCenter에 포함하지 않음 | SRP 원칙. 동적 상태는 별도 Subsystem/GameState에서 관리 |
| 매니페스트 참조 방식 | 초기 TObjectPtr, 50개 이상 시 TSoftObjectPtr 전환 | 현재 규모(10개 미만)에서 비동기 로딩은 오버엔지니어링 |
| 내부 저장소 구현 | 초기 자체 TMap, 50개 이상 시 AssetManager 래핑 검토 | 현재 규모에서 AssetManager 래핑은 복잡도만 증가 |
| Config 내부 구조 | USTRUCT 기본, 다형성 필요 시 해당 섹션만 Instanced UObject 전환 | 현재는 수치 튜닝이지 알고리즘 교체가 아님 |
| IsDataValid 구현 위치 | C++ 단일 관리, BP 확장 없음 | 1인 개발에서 검증 로직 분산은 역효과 |
| 에러 알림 수단 | ensureMsgf + AddOnScreenDebugMessage | Fatal은 에디터 크래시. ensure는 경고만 |
| API 안정성 | 외부 API 시그니처 불변, 내부만 교체 가능 | 미래 마이그레이션 비용 최소화 |

### 10.2 추후 논의 필요 항목

1. **ExCore 범용 Config의 구체적 항목 목록**: UI 글로벌 테마, 볼륨 리미트 등 완전히 글로벌한 메타 설정만 포함. 구체적으로 어떤 프로퍼티를 DA_ExConfig_Core에 넣을지 항목 확정 필요

2. **팀 확장 시 Config DA 분리 정책**: 현재 1인 개발이므로 모듈당 Config DA 1개로 통합하지만, 추후 팀 확장 시 Git 충돌 방지를 위해 분야별 분리(예: DA_Runner_Gameplay, DA_Runner_LevelDesign) 검토

3. **커스텀 에디터 유틸리티 제작 시점**: 데이터가 50개 이상으로 증가하는 시점에 "Ex Data Browser" EditorUtilityWidget 제작 여부 판단

4. **BGM/비주얼 대형 에셋의 비동기 로딩**: ExBGMTrackDataAsset, ExMusicPhaseDataAsset 등 대형 리소스를 참조하는 Definition의 로딩 전략. DataCenter 캐싱과 AssetManager 비동기 로딩의 연동 방식 상세 설계

---

*본 문서는 승인 후 구현을 시작하며, 구현 과정에서 변경 사항이 발생하면 즉시 업데이트한다.*
