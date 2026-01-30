# ExFrameWork

**ExFrameWork**는 언리얼 엔진 5 기반의 모듈형 게임플레이 프레임워크 프로젝트입니다.
**LyraStarterGame**의 GameFeatures 시스템 및 Modular Gameplay 아키텍처와, **GameAnimationSample** (Sandbox Project)의 최신 애니메이션/캐릭터 로직을 기반으로 재작성되었습니다.
확장성과 유지보수성이 뛰어난 구조를 지향합니다.

## 🛠 주요 특징 (Key Features)

### 1. GameFeatures 시스템 (Modular Gameplay)
- **ExCore 플러그인**: 프로젝트의 핵심 기능을 담은 GameFeature 플러그인입니다. (`Plugins/GameFeatures/ExCore`)
- **Action 기반 확장**: GameFeatureData를 통해 컴포넌트 추가, 치트 등록, 데이터 테이블 로드 등을 데이터 기반으로 처리합니다.

### 2. 향상된 캐릭터 스폰 시스템 (Container-Based Spawning)
- **Container Structure**: 이동과 물리 처리를 담당하는 '껍데기(Container)' 폰과, 실제 비주얼/로직을 담당하는 '로컬 플레이어' 액터를 분리했습니다. ((Ref: `GameAnimationSample`)
- **ExGameMode**: `SpawnLocalPlayer` 함수를 통해 런타임에 캐릭터 비주얼을 자유롭게 교체하고 부착하는 로직을 구현했습니다.
- **Data Driven**: `ExGameModeDataSet`을 통해 컨테이너 클래스와 로컬 플레이어 클래스를 데이터 에셋으로 관리합니다.

### 3. 개발 가이드라인 (Development Guidelines)
프로젝트의 코드 품질과 일관성을 위해 아래 가이드라인 문서를 반드시 준수해야 합니다.
- 📄 **가이드라인 문서**: [`Md/ExFrameWork_Guidelines.md`](Md/ExFrameWork_Guidelines.md)
- **주요 내용**:
    - 네이밍 규칙 (`Ex` 접두사, `b` Boolean 등)
    - UE5 베스트 프랙티스 (`TObjectPtr`, Assertions)
    - 네트워크/멀티플레이어 고려 사항

## 📊 현재 구현 단계 (Current Status)

### ✅ Phase 1: 기본 프레임워크 구축 (완료)
- [x] **Project Setup**: `ModularGameplay`, `GameFeatures` 플러그인 활성화 및 설정.
- [x] **ExCore Plugin**: GameFeature 플러그인 생성 및 `PoseSearch`, `Chooser` 의존성 구성.
- [x] **Custom GameMode**: `AExGameMode` 및 `UExGameModeDataSet` 구현.
- [x] **Spawn System**: `ContainerPawn` + `LocalPlayerActor` 부착 방식의 스폰 로직 구현 (Mesh Hiding 포함).

### 🚧 Phase 2: 애니메이션 시스템 (진행 중)
- [ ] **Motion Matching**: `ExCore` 내 Pose Search Schema 및 Database 구성.
- [ ] **Character Logic**: `SandboxCharacter_CMC` 분석 및 로직 이식.

## 📂 프로젝트 구조 (Structure)

- **Source/**: C++ 소스 코드 (Core Logic)
- **Plugins/GameFeatures/ExCore/**: 핵심 게임플레이 기능 (Motion Matching 등 포함)
- **Md/**: 프로젝트 분석 및 가이드라인 문서

## 🚀 시작하기 (Getting Started)

1.  **플러그인 확인**: `ModularGameplay`, `GameFeatures`, `ExCore` 플러그인이 활성화되어야 합니다.
2.  **데이터 설정**: `DefaultGame.ini`의 AssetManager 설정이 `GameFeatureData`를 스캔하도록 구성되어 있습니다.
