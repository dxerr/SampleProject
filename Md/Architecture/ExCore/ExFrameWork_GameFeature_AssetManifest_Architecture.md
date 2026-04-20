# GameFeature 에셋 매니페스트 시스템 아키텍처

ExCore 모듈에 구축된 GameFeature 플러그인 에셋 패키징 매니페스트 시스템의 구조와 사용법을 설명합니다.

---

## 1. 개요

### 목적
- **패키징 자동화**: GameFeature 플러그인이 활성화될 때만 해당 맵/에셋이 빌드에 포함되도록 제어
- **통합 관리**: 플러그인별 에셋 목록을 한 곳(Data Asset)에서 일괄 등록/관리
- **재사용성**: ExCore의 공통 베이스를 상속받아, 어떤 GameFeature 플러그인이든 최소 코드로 즉시 적용

### 설계 배경
언리얼 엔진의 Asset Manager는 `UWorld(Map)` 타입을 내부적으로 **Editor Only**로 취급하므로,
GameFeatureData에서 Map 타입을 직접 스캔하면 패키징 크래시(`AssetManager.cpp Ensure 조건 실패`)가 발생합니다.

이를 해결하기 위해 **일반 PrimaryDataAsset을 "에셋 명부(Manifest)"로 사용**하고,
해당 명부가 맵에 대한 Soft 레퍼런스를 가지도록 하여 쿠킹 의존성 체인을 형성합니다.

### 적용된 설계 패턴
Lyra 프로젝트의 `LyraUserFacingExperienceDefinition` 패턴을 참고하여,
`UPrimaryDataAsset` 기반의 매니페스트 데이터 에셋으로 플러그인 에셋을 관리합니다.

---

## 2. 클래스 계층 구조

```
UPrimaryDataAsset (Engine)
└── UExFeatureAssetManifest           [ExCore - 공통 베이스, Abstract]
    ├── UExRunnerAssetManifest        [ExRunnerPlay - Runner 전용]
    └── UEx????AssetManifest          [미래 플러그인 - 한 줄 상속으로 확장]
```

---

## 3. 파일 구조

```
ExCore/Source/ExCoreRuntime/
└── Data/
    ├── ExFeatureAssetManifest.h      // 공통 베이스 클래스 (핵심)
    └── ExFeatureAssetManifest.cpp

ExRunnerPlay/Source/ExRunnerPlayRuntime/
└── Data/
    ├── ExRunnerMapManifest.h         // Runner 전용 서브클래스
    └── ExRunnerMapManifest.cpp
```

---

## 4. 핵심 클래스 API

### UExFeatureAssetManifest (공통 베이스)

| 프로퍼티 | 타입 | 설명 |
|---------|------|------|
| `FeatureMaps` | `TArray<TSoftObjectPtr<UWorld>>` | 패키징에 포함할 맵(.umap) 목록. 에디터에서 맵만 선택 가능 |
| `AdditionalAssets` | `TArray<FSoftObjectPath>` | 맵 외 추가 포함 에셋 경로 목록 (데이터테이블, 사운드 등) |
| `ManifestDescription` | `FText` | 에디터 표시용 설명 메모 |

> **Abstract 클래스**: 에디터에서 직접 `UExFeatureAssetManifest` 기반으로 데이터 에셋을 만들 수 없습니다. 반드시 플러그인별 서브클래스를 통해서만 생성해야 합니다.

---

## 5. 새 GameFeature 플러그인에 적용하는 방법

### Step 1 — 서브클래스 C++ 파일 생성

```cpp
// MyPlugin/Source/MyPluginRuntime/Data/MyPluginAssetManifest.h
#pragma once
#include "ExFeatureAssetManifest.h"
#include "MyPluginAssetManifest.generated.h"

UCLASS(BlueprintType)
class MYPLUGINRUNTIME_API UMyPluginAssetManifest : public UExFeatureAssetManifest
{
    GENERATED_BODY()

    // 플러그인 전용 필드가 필요하면 여기에 추가
    // 기본 FeatureMaps, AdditionalAssets은 부모 클래스에서 상속됨
};
```

```cpp
// MyPlugin/Source/MyPluginRuntime/Data/MyPluginAssetManifest.cpp
#include "MyPluginAssetManifest.h"
```

### Step 2 — 데이터 에셋 생성

1. 언리얼 에디터 Content Browser에서 우클릭
2. **Miscellaneous(기타) → Data Asset** 선택
3. 부모 클래스로 `MyPluginAssetManifest` 검색 후 선택
4. 에셋 이름 예시: `DA_MyPluginMaps`
5. 에셋 열기 → `FeatureMaps` 배열에 포함시킬 맵 파일들을 추가

### Step 3 — GameFeatureData 에셋 등록

1. 플러그인의 `GameFeatureData` 에셋 열기
2. **Primary Asset Types to Scan** 항목에서 `+` 클릭
3. 다음과 같이 설정:
   - **Primary Asset Type**: `MyPluginAssetManifest` (임의의 식별 텍스트)
   - **Asset Base Class**: `MyPluginAssetManifest` (Step 1에서 만든 C++ 클래스)
   - **Directories**: `DA_MyPluginMaps` 에셋이 저장된 폴더 경로 (예: `/MyPlugin/Data`)
   - **Cook Rule**: **`Always Cook`** ← 이 설정이 핵심!
4. 저장

> **결과**: 해당 플러그인이 활성화된 상태로 패키징하면 `DA_MyPluginMaps`가 먼저 쿠킹되고, 그 안에 등록된 맵들이 의존성 체인으로 자동 포함됩니다.

---

## 6. 현재 등록된 플러그인 매니페스트 현황

| 플러그인 | 클래스 | 데이터 에셋 | 포함 맵 |
|---------|--------|-----------|--------|
| ExRunnerPlay | `UExRunnerAssetManifest` | `DA_RunnerMaps` | L_Lobby, L_ExRunnerTest |

---

## 7. 주의사항 및 FAQ

### Q: 왜 GameFeatureData에서 Map 타입을 직접 사용하면 안 되나요?
언리얼 엔진 내부적으로 `UWorld(Map)` 타입은 `bIsEditorOnly=True`로 고정되어 있습니다.
이를 `False`로 강제하면 `AssetManager.cpp:1344` 라인의 Ensure 조건이 실패하며 패키징이 중단됩니다.
매니페스트 방식은 이 제약을 PrimaryDataAsset 레이어에서 우회하는 공식적인 대안입니다.

### Q: 빌드 PC에서만 에러가 나는 이유는?
라이브 코딩(`Ctrl+Alt+F11`)은 에디터 메모리에만 패치를 적용합니다.
패키징(UAT)은 디스크의 실제 DLL을 읽으므로, **반드시 전체 빌드(IDE 빌드)를 선행**해야 합니다.

### Q: 맵을 추가할 때마다 C++ 코드를 수정해야 하나요?
**아닙니다.** 에디터에서 해당 플러그인의 데이터 에셋(예: `DA_RunnerMaps`)을 열고
`FeatureMaps` 배열에 `+` 버튼으로 추가하면 됩니다. C++ 수정은 불필요합니다.

---

## 8. 연관 문서

- [ExCore 이벤트 시스템](./ExFrameWork_EventSystem_Architecture.md)
- [구현 계획서](../../Plans/GameFeature_AssetManifest_Plan.md)

---

*최종 수정: 2026-04-17*
