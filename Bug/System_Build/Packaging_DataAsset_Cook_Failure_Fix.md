# 패키징 빌드 시 데이터 에셋(DataAsset) 누락 현상 분석 및 해결 보고서

> **분류**: 빌드 및 패키징 (System_Build)  
> **엔진**: UE 5.7.3  
> **보고자**: Antigravity (AI Coding Assistant)  
> **보고 대상**: 주인님 (Master)  

---

## 1. 현상 요약 (Problem)
Windows 및 모바일(Android/iOS) 실기 패키징 빌드 실행 시, 런타임에 `ExRunnerConfig`와 `ExRunnerItemSpawnTable` 에셋을 찾을 수 없다는 붉은색 오류 경고가 화면에 도배되며 게임 구동 및 아이템 스폰 테이블 매칭 로직이 오동작함.
* 에디터(PIE) 환경에서는 완벽하게 정상 동작하지만, 패키징 완료 후 Standalone 환경에서만 해당 에셋들이 소실됨.

---

## 2. 원인 분석 (Root Cause)

### ① 에셋 매니저(Asset Manager)의 스캔 클래스 필터링 불일치
주인님께서 에디터에서 설정하신 `Primary Asset Types to Scan` 설정을 보면:
* **Asset Base Class**: `ExRunnerAssetManifest` (C++ 클래스)
* **Directories**:
  * `[0] /ExRunnerPlay/Data/PrimaryDataAsset` (매니패스트 에셋 위치)
  * `[1] /ExRunnerPlay/Data/Config` (설정 데이터 에셋 위치)
  * `[2] /ExRunnerPlay/Data/Presets` (스폰 테이블 데이터 에셋 위치)

여기서 아주 치명적인 스캔 규칙 불일치가 일어났습니다.
언리얼 에셋 매니저의 `Primary Asset Types to Scan`은 지정된 경로(`Directories`) 내에서 **오직 `Asset Base Class`로 지정한 클래스(여기서는 `ExRunnerAssetManifest`)와 일치하거나 상속받은 에셋들만** 선별하여 Primary Asset으로 등록합니다.

* `/ExRunnerPlay/Data/Config`에 존재하던 `DA_ExConfig_Runner` 에셋의 클래스는 `ExRunnerConfig` (`UExConfigDataAsset` 상속)입니다.
* `/ExRunnerPlay/Data/Presets`에 존재하던 `DA_ExPreset_RunnerSpawnTable` 에셋의 클래스는 `ExRunnerItemSpawnTable` (`UExPresetDataAsset` 상속)입니다.

이 두 에셋 모두 `ExRunnerAssetManifest` 클래스를 상속받은 클래스가 **아닙니다**. 따라서 에셋 매니저는 해당 폴더들을 탐색하였으나, 조건(클래스 필터)이 일치하지 않아 **단 하나의 에셋도 스캔(등록)하지 않고 완전히 무시(Ignore)**했습니다.

### ② 언리얼 에셋 쿠킹(Asset Cooking)의 배제 정책
언리얼 쿠커(Cooker)는 용량 최소화를 위해 활성 맵에서 추적되는 하드 레퍼런스 체인에 엮여 있지 않거나, 에셋 매니저에 의해 공식 Primary Asset으로 지정 및 강제 쿠킹 룰(`Always Cook`)이 잡혀있지 않은 에셋들은 패키징 빌드(`.pak`)에서 자동으로 배제(Strip/Exclude)시킵니다.
따라서 스캔에서 누락된 `Config`와 `Preset` 데이터 에셋들은 최종 빌드 패키지에서 완전히 소실되어 런타임 Null 에러를 발생시켰습니다.

---

## 3. 해결 방안 (Solution Options)

주인님, 본 매니패스트 아키텍처 가이드라인이 권장하는 **[1안]**으로 해결하시는 것을 강력하게 추천해 드립니다.

### 💡 [1안] 매니패스트 데이터 에셋에 하드 레퍼런스 등록 (아키텍처 권장)
개별 에셋 클래스마다 에셋 매니저에 스캔 타입을 추가해 관리하는 복잡함을 없애고, 플러그인당 단 하나의 매니패스트 에셋을 거쳐 연관 에셋들을 일괄 쿠킹하는 정석 방법입니다.

1. **Asset Manager 설정 복원**:
   * `GameFeatureData` 에셋의 `Asset Manager` -> `Primary Asset Types to Scan` -> `Directories`에서 주인님께서 새로 임시 추가하셨던 `[1] /ExRunnerPlay/Data/Config` 와 `[2] /ExRunnerPlay/Data/Presets` 두 개 경로를 **삭제**하여 원래대로 **`[0] /ExRunnerPlay/Data/PrimaryDataAsset` 하나만** 남겨 둡니다.
2. **매니패스트 에셋 열기**:
   * Content 브라우저에서 `/ExRunnerPlay/Data/PrimaryDataAsset` 경로로 이동한 뒤, 실제로 존재하는 매니패스트 에셋인 **`DA_RunnerMaps`**를 더블클릭하여 엽니다.
3. **추가 에셋 등록**:
   * 에셋 내부의 **`추가 에셋 목록 (Additional Assets)`** 배열 프로퍼티를 찾습니다.
   * 이 배열 엘리먼트로 쿠킹에서 소실되었던 아래 데이터 에셋들을 드래그 앤 드롭 또는 선택하여 등록해 줍니다:
     * 🟢 `DA_ExConfig_Runner` (Config 데이터)
     * 🟢 `DA_ExPreset_RunnerSpawnTable` (스폰 테이블 Preset 데이터)
     * *(이외에 추가적으로 데이터센터에 탑재할 정의 데이터 에셋(DA_ExObstacle 등)이 있다면 동일하게 이 배열에 등록해주시면 됩니다.)*
4. **결과**: `DA_RunnerMaps` 에셋이 Primary Asset으로 스캔 및 로드될 때, 매니패스트에 의해 **하드 레퍼런스(Hard Reference) 체인**으로 엮인 `DA_ExConfig_Runner`와 `DA_ExPreset_RunnerSpawnTable` 에셋 또한 쿠커가 무조건 감지하고 패키지에 강제 포함시키게 됩니다.

---

### 💡 [2안] 에셋 매니저에서 타입별 자동 스캔 설정 추가 (개별 자동 스캔 방식)
매니패스트 에셋을 거치지 않고 에셋 매니저가 특정 폴더 내의 해당 클래스 에셋들을 자동으로 다 긁어가게 설정하고 싶다면, `Primary Asset Types to Scan` 배열 원소 자체를 **클래스별로 여러 개 추가**해주어야 합니다.

1. `Primary Asset Types to Scan` 우측의 `+ (Add Element)` 버튼을 두 번 더 클릭하여 총 3개의 스캔 항목을 만듭니다.
2. **[Index 0] (기존 매니패스트 스캔)**
   * `Primary Asset Type`: `ExRunnerMapManifest`
   * `Asset Base Class`: `ExRunnerAssetManifest`
   * `Directories`: `/ExRunnerPlay/Data/PrimaryDataAsset`
3. **[Index 1] (신규 Config 자동 스캔)**
   * `Primary Asset Type`: `ExRunnerConfig`
   * `Asset Base Class`: `ExRunnerConfig` (또는 부모 클래스인 `ExConfigDataAsset`)
   * `Directories`: `/ExRunnerPlay/Data/Config`
   * `Rules` -> `Cook Rule`을 반드시 **`Always Cook`**으로 설정
4. **[Index 2] (신규 Preset 자동 스캔)**
   * `Primary Asset Type`: `ExRunnerPreset`
   * `Asset Base Class`: `ExPresetDataAsset` (혹은 `ExRunnerItemSpawnTable`)
   * `Directories`: `/ExRunnerPlay/Data/Presets`
   * `Rules` -> `Cook Rule`을 반드시 **`Always Cook`**으로 설정

---

## 4. 최종 결론
주인님께서 에디터에서 `/ExRunnerPlay/Data/Config`와 `/ExRunnerPlay/Data/Presets` 폴더 경로만을 `Directories`에 단순히 얹어두는 방식으로는 **Asset Base Class 필터링 규격 불일치로 인해 에셋이 전혀 스캔되지 않았음**을 명확히 확인했습니다.

**[1안]**으로 처리하시는 것이 에셋 관리 구조상 가장 단순하고 깨끗하며, `ExCore` 범용 매니패스트 아키텍처 설계와 100% 일치하는 정석 해결책입니다, Master!
