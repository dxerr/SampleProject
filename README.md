# ExFrameWork

**ExFrameWork**는 언리얼 엔진 5 기반의 모듈형 게임플레이 프레임워크 프로젝트입니다.
**LyraStarterGame**의 GameFeatures 시스템 및 Modular Gameplay 아키텍처와, **GameAnimationSample** (Sandbox Project)의 최신 애니메이션/캐릭터 로직을 기반으로 재작성되었습니다.
확장성과 유지보수성이 뛰어난 구조를 지향합니다.

## 🛠 주요 특징 (Key Features)

### 1. GameFeatures 시스템 (Modular Gameplay)
- **ExCore 플러그인**: 프로젝트의 핵심 기능을 담은 GameFeature 플러그인입니다. (`Plugins/GameFeatures/ExCore`)
- **Action 기반 확장**: GameFeatureData를 통해 컴포넌트 추가, 치트 등록, 데이터 테이블 로드 등을 데이터 기반으로 처리합니다.

### 2. 향상된 캐릭터 스폰 시스템 (Visual Override System)
- **Container Structure**: 이동과 물리 처리를 담당하는 '컨테이너(Container)' 폰과, 실제 비주얼/로직을 담당하는 'Visual Override' 액터를 분리했습니다. (Ref: `GameAnimationSample`)
- **AExCoreGameMode**: ExCore 플러그인 내 독립적인 게임 모드로, `ApplyVisualOverride` 및 `ChangeVisualOverride` 함수를 통해 런타임에 캐릭터 비주얼을 자유롭게 교체합니다.
- **UExCoreSpawnDataAsset**: Pawn 클래스 목록과 Visual Override 클래스 목록을 데이터 에셋으로 관리합니다.
- **UExVisualOverrideComponent**: Visual Override를 관리하는 컴포넌트로, 캐릭터에 부착하여 비주얼 적용/제거를 처리합니다.

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
- [x] **Custom GameMode**: `AExCoreGameMode` 및 `UExCoreSpawnDataAsset` 구현 (ExCore 플러그인 내).
- [x] **Visual Override System**: `UExVisualOverrideComponent`를 통한 비주얼 교체 시스템 구현.
- [x] **Spawn System**: `ContainerPawn` + `VisualOverride` 부착 방식의 스폰 로직 구현 (Mesh Hiding 포함).

### 🚧 Phase 2: 애니메이션 시스템 (진행 중)
- [ ] **Motion Matching**: `ExCore` 내 Pose Search Schema 및 Database 구성.
- [ ] **Character Logic**: `SandboxCharacter_CMC` 분석 및 로직 이식.

## 📂 프로젝트 구조 (Structure)

```
ExFrameWork/
├── Source/                          # C++ 소스 코드 (Core Logic)
├── Plugins/GameFeatures/ExCore/     # 핵심 게임플레이 기능
│   └── Source/ExCoreRuntime/
│       ├── GameModes/               # AExCoreGameMode
│       ├── Data/                    # UExCoreSpawnDataAsset
│       └── Components/              # UExVisualOverrideComponent
└── Md/                              # 프로젝트 분석 및 가이드라인 문서
```

## 🚀 시작하기 (Getting Started)

1.  **플러그인 확인**: `ModularGameplay`, `GameFeatures`, `ExCore` 플러그인이 활성화되어야 합니다.
2.  **데이터 설정**: `DefaultGame.ini`의 AssetManager 설정이 `GameFeatureData`를 스캔하도록 구성되어 있습니다.
3.  **Visual Override 설정**: `DA_ExCoreSpawnData` 데이터 에셋에서 Pawn 클래스와 Visual Override 클래스를 설정합니다.

## 📝 최근 변경 사항 (Recent Changes)

### 2026-02-01: ExCore 플러그인 리팩토링
- **삭제**: `Source/ExFrameWork/Modes/ExGameMode` (ExFramework 모듈의 게임 모드)
- **삭제**: `ExCore/Content/Animation/` 내 Motion Matching 에셋 (재구성 예정)
- **삭제**: `ExCore/Content/BluePrint/` 내 기존 블루프린트 에셋
- **추가**: `ExCore/Source/ExCoreRuntime/GameModes/ExCoreGameMode` - 독립적인 게임 모드
- **추가**: `ExCore/Source/ExCoreRuntime/Data/ExCoreSpawnDataAsset` - 스폰 설정 데이터 에셋
- **추가**: `ExCore/Source/ExCoreRuntime/Components/ExVisualOverrideComponent` - Visual Override 관리 컴포넌트
- **추가**: `BP_ExCoreGameMode`, `DA_ExCoreSpawnData` 블루프린트 에셋
