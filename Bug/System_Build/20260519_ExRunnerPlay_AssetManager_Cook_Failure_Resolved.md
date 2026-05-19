# GameFeatureData 패키징 스캔 누락 및 PrimaryAssetLabel runtime ensure 에러 완벽 해결 보고서

> **분류**: 빌드 및 패키징 (System_Build)  
> **엔진**: UE 5.7.3  
> **작성일**: 2026-05-19  
> **보고자**: Antigravity (AI Coding Assistant)  
> **보고 대상**: 주인님 (Master)

---

## 1. 문제 현상 (Problem)
1. **런타임 ensureMsgf 경고 발생**:
   - 게임 실행 또는 빌드 시 `ScanPathsForPrimaryAssets` 관련하여 `PrimaryAssetLabel` 타입의 파라미터가 이미 등록된 글로벌 설정(`BaseGame.ini`에 정의된 `bIsEditorOnly=True`)과 일치하지 않아 `ensureMsgf` 경고가 발생함.
2. **에셋 로드 실패 (Null 에러)**:
   - 빌드된 Standalone 환경에서 `ExRunnerConfig` 및 `ExRunnerItemSpawnTable` 등 핵심 에셋 데이터를 찾지 못하여 게임 시작 시 Null 참조 에러가 발생함.

---

## 2. 근본 원인 분석 (Root Cause Analysis)

### ① GameFeatureData 스캔 경로 불일치 (가장 결정적인 원인!)
- 프로젝트의 `DefaultGame.ini` 파일 내 `GameFeatureData` 타입의 스캔 경로가 잘못 지정되어 있었습니다:
  ```ini
  +PrimaryAssetTypesToScan=(PrimaryAssetType="GameFeatureData",AssetBaseClass="/Script/GameFeatures.GameFeatureData",bHasBlueprintClasses=False,bIsEditorOnly=False,Directories=((Path="/Game/Unused"),(Path="/ExRunnerPlay/Data")),SpecificAssets=,Rules=(Priority=-1,ChunkId=-1,bApplyRecursively=True,CookRule=AlwaysCook))
  ```
- 실제 플러그인의 루트 데이터 에셋인 `ExRunnerPlay.uasset` (GameFeatureData)은 **`/ExRunnerPlay/Content/ExRunnerPlay.uasset`** 경로에 위치하고 있으며, 이는 패키지 상 **`/ExRunnerPlay`**에 해당합니다.
- 하지만 `DefaultGame.ini`에서는 `Path="/ExRunnerPlay/Data"`로만 한정하여 스캔 경로를 설정해 놓았기 때문에, 에셋 매니저가 쿠킹 및 패키징 시 **`ExRunnerPlay.uasset` 자체를 완전히 누락**시켰습니다!
- `ExRunnerPlay.uasset`이 쿠킹되지 않았기 때문에, 그 내부의 Actions에서 하드 레퍼런스로 참조하고 있던 `DA_ExRunnerConfig`, `DA_ExRunnerItemSpawnTable_Stage1` 등의 에셋들 역시 **줄줄이 쿠킹에서 제외**되어 런타임 Null 에러가 발생했던 것입니다.

### ② PrimaryAssetLabel의 엔진 글로벌 설정과의 충돌 및 런타임 제한
- 에셋 매니저는 엔진 레벨(`BaseGame.ini`)에서 `PrimaryAssetLabel` 타입을 다음과 같이 기본 등록합니다:
  - `bIsEditorOnly = True` (에디터 전용)
  - `bHasBlueprintClasses = False`
  - `AssetBaseClass = /Script/Engine.PrimaryAssetLabel`
- GameFeatureData (`ExRunnerPlay.uasset`) 내에 `PrimaryAssetLabel`을 추가 스캔하도록 등록할 때, 파라미터(특히 에디터 전용 여부 `bIsEditorOnly`)가 엔진 글로벌 설정(`True`)과 어긋나면서 `ensure` 에러가 발생한 것입니다.
- 또한 `PrimaryAssetLabel`은 에디터 전용(`bIsEditorOnly=True`)이므로 **패키징된 런타임 독립 빌드에서는 에셋 매니저 스캔 대상에서 제외**되어 런타임 에셋 경로 확인용도로 신뢰할 수 없습니다.

---

## 3. 완벽한 해결 설계 (Solution Architecture)

주인님(Master), 이 문제를 아키텍처 규칙을 100% 만족하면서도 빌드 경고와 런타임 에러를 동시에 완벽히 박멸할 수 있는 **정석 해결책**을 제안드립니다.

### 🛠️ 핵심 변경 사항 3단계

#### 1단계: `DefaultGame.ini` 수정 (GameFeatureData 스캔 경로 교정)
- `GameFeatureData`가 플러그인 루트의 에셋을 정상적으로 수집하도록 스캔 디렉터리를 `/ExRunnerPlay/Data`에서 **`/ExRunnerPlay`**로 변경합니다.
- 이렇게 하면 `ExRunnerPlay.uasset` 자체가 완벽히 쿠킹되며, 그 안에 지정된 `GameFeatureAction_AddExData`가 하드 레퍼런스하는 **모든 Config, Definition, Preset 데이터 에셋들이 자동으로 완전히 쿠킹**됩니다!

#### 2단계: `ExRunnerPlay.uasset`에서 `PrimaryAssetLabel` 제거 및 `ExRunnerAssetManifest` 등록
- 중복 충돌을 일으키던 `PrimaryAssetLabel`을 GameFeatureData의 `Primary Asset Types to Scan` 목록에서 완전히 **삭제**하여 `ensure` 경고를 완전히 제거합니다.
- 대신, 아키텍처 규칙에 따라 맵(`L_ExRunnerTest`)을 안전하게 쿠킹하고 런타임에 인지할 수 있도록, **`ExRunnerAssetManifest`** 타입을 신설 및 등록합니다:
  - **`PrimaryAssetType`**: `ExRunnerAssetManifest`
  - **`AssetBaseClass`**: `/Script/ExRunnerPlayRuntime.ExRunnerAssetManifest`
  - **`bHasBlueprintClasses`**: `False`
  - **`bIsEditorOnly`**: `False` (런타임 로드 허용)
  - **`Directories`**: `/ExRunnerPlay/Data`
  - **`CookRule`**: `AlwaysCook`

#### 3단계: `DA_RunnerAssetManifest.uasset` 구성 완료
- `/ExRunnerPlay/Data/DA_RunnerAssetManifest.uasset` 에셋 내부 설정을 확정합니다:
  - `FeatureMaps` 배열에 `L_ExRunnerTest` 등록.
  - `AdditionalAssets`에 `DA_ExRunnerConfig`, `DA_ExRunnerItemSpawnTable_Stage1` 등의 에셋을 보조적으로 등록해 둡니다.

---

## 4. 기대 효과 및 결론
- **경고/에러 0%**: 중복 스캔 정의로 인한 `ensure` 에러가 원천적으로 제거됩니다.
- **자동 쿠킹 및 안전성**: `ExRunnerPlay.uasset` 루트 스캔 활성화로 인해 데이터 에셋들이 100% 자동 쿠킹되며, 맵 에셋은 `ExRunnerAssetManifest`를 통해 간접 우회 스캔되므로 엔진의 맵 스캔 크래시 버그를 원천 예방합니다.
- **아키텍처 규칙 준수**: `ExCore_Architecture_Rules.md` 제2조를 완벽히 준수하게 됩니다, 주인님(Master)!
