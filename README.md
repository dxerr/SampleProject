# ExFrameWork

**Unreal Engine 5.7** 기반의 **모듈형 모바일 러너(Runner) 게임 프레임워크**.  
**GameFeatures 플러그인 아키텍처**로 Core/Feature를 완전 분리하여, 엔진 수준의 확장성과 프로덕션 품질을 동시에 추구합니다.

> **Target Platform:** Mobile (Android 우선 개발, iOS 적용 예정)  
> **Engine:** Unreal Engine 5.7  
> **Architecture:** GameFeatures Plugin (ExCore + ExRunnerPlay)

---

## Architecture Overview

```
ExFrameWork/
├── Source/ExFrameWork/              # 루트 모듈 (엔진 진입점, 기본 타입)
│
├── Plugins/GameFeatures/
│   ├── ExCore/                      # 핵심 프레임워크 (항상 활성)
│   │   └── Source/ExCoreRuntime/
│   │       ├── GameModes/           # AExGameModeBase, AExCoreGameMode
│   │       ├── Subsystems/          # UIManager, DataCenter, MusicManager
│   │       ├── UI/                  # CommonUI 위젯 체계 (Modal, Popup, Toast)
│   │       ├── Events/              # GameplayTag 기반 이벤트 시스템
│   │       ├── Data/                # DataCenter 3-Base DataAsset 체계
│   │       ├── Components/          # VisualOverride, DebugState, PathManager
│   │       ├── Experience/          # Experience Definition / GameFeature Action
│   │       ├── Debug/               # 치트 확장 시스템
│   │       └── Items/               # 아이템 스폰 매니저 베이스
│   │
│   └── ExRunnerPlay/                # 러너 게임 피처 (게임별 로직)
│       └── Source/ExRunnerPlayRuntime/
│           ├── GameModes/           # AExRunnerGameMode
│           ├── GameStates/          # AExRunnerGameState (Replicated 트랙 데이터)
│           ├── Movement/            # Mover 2.0 기반 러너 이동 컴포넌트
│           ├── InputStrategies/     # Strategy Pattern 입력 모드 (Manual/AutoRun/AutoButtonRun)
│           ├── Components/          # ChunkSpawner, ObstacleManager, InputComponent
│           ├── Rules/               # GameRule 시스템 (서버 권한 판정)
│           ├── Actors/              # FloorChunk, Obstacle, CurvedFloor
│           └── UI/                  # 러너 전용 HUD, ViewModel (MVVM)
│
└── Md/                              # 아키텍처 문서, 개발 계획, 가이드
    ├── Architecture/ExCore/         # ExCore 시스템 아키텍처 문서
    ├── Architecture/ExRunnerPlay/   # ExRunnerPlay 시스템 아키텍처 문서
    ├── Plans/                       # 구현 계획서
    ├── Guides/                      # 셋업 가이드
    └── Bug/                         # 이슈 해결 기록 (KI)
```

### Core/Feature 분리 원칙

ExCore는 ExRunnerPlay를 **절대 참조하지 않습니다.** 모든 교차 통신은 GameplayTag 기반 이벤트 또는 인터페이스를 통해 이루어집니다.

---

## Key Technical Systems

### ExCore (Framework Layer)

| 시스템 | 핵심 기술 | 문서 |
|--------|----------|------|
| **DataCenter** | GameInstance Subsystem, 3-Base DataAsset (Config/Definition/Preset), 복합키 저장소, `TWeakObjectPtr` 캐싱 | `Architecture/ExCore/ExFrameWork_DataCenter_Architecture.md` |
| **UI System** | CommonUI (`UCommonActivatableWidget`), MVVM ViewBinding, 위젯 3분류 (Screen/Modal/HUD), Descriptor 기반 Popup/Toast | `Architecture/ExCore/ExFrameWork_UI_System_Architecture.md` |
| **Event System** | `FGameplayTag` + `FExGameplayEventPayload`, Dynamic Multicast Delegate, `CreateWeakLambda` 필수 | `Architecture/ExCore/ExFrameWork_EventSystem_Architecture.md` |
| **Sound/BGM** | Quartz + MetaSounds, `UExMusicManagerSubsystem` (GameInstance), Phase별 레이어 볼륨 프리셋, Beat 동기화 | `Architecture/ExCore/ExFrameWork_Sound_System_Architecture.md` |
| **Multiplayer Flow** | `AExGameModeBase`/`AExGameStateBase` 분리, `EExMatchPhase` 상태 머신, Seamless Travel, PlayerState Replicated Score | `Architecture/ExCore/ExFrameWork_Multiplayer_Flow_Architecture.md` |
| **Input System** | Enhanced Input + MVVM, `InjectInputForAction` 파이프라인, 하드웨어/UI 입력 통합 | `Architecture/ExCore/ExFrameWork_Input_System_Architecture.md` |
| **GameFeature Asset** | AssetManifest 패턴, `DirectoriesToAlwaysCook`, Cook-time vs Runtime INI 분리 | `Architecture/ExCore/ExFrameWork_GameFeature_AssetManifest_Architecture.md` |
| **Mover 2.0** | `UMoverComponent` 기반 이동, 클라이언트 예측 프레임워크, `ProduceInput` 패턴 | `Architecture/ExCore/Mover_System_Analysis.md` |

### ExRunnerPlay (Game Feature Layer)

| 시스템 | 핵심 기술 | 문서 |
|--------|----------|------|
| **Runner Core** | 무한 트랙 생성, SplineMesh 기반 커브드 바닥, 오브젝트 풀링, 거리 기반 난이도 스케일링 | `Architecture/ExRunnerPlay/ExRunner_System_Architecture.md` |
| **Curved Floor** | `USplineMeshComponent` 동적 생성, Bézier 커브 보간, 비균일 스케일 보정 | `Architecture/ExRunnerPlay/ExRunner_CurvedFloor_System_Architecture.md` |
| **Obstacle System** | Definition 기반 데이터 드리븐, `FExObstacleContext` 스폰 컨텍스트, Z-Position 자동 계산 | `Architecture/ExRunnerPlay/ExRunner_Obstacle_System_Architecture.md` |
| **Item System** | 하이브리드 Effect-as-Object + Tag-Driven, 오브젝트 풀링, `CalculateItemZ()` 자동 배치 | `Architecture/ExRunnerPlay/ExFrameWork_Item_System_Architecture.md` |
| **Input Strategy** | Strategy Pattern (Manual/AutoRun/AutoButtonRun), `WidgetSwitcher` HUD 전환, 쿨다운 독립 제어 | `Plans/ExRunner_InputSystem_Plan.md` |
| **Multiplayer** | 결정론적 스폰 (시스템별 독립 `FRandomStream`), 스폰 도메인 매트릭스, Join-in-Progress 스냅샷 | `Architecture/ExRunnerPlay/Multiplayer_Network_Readiness_Analysis.md` |

### Character & Animation

> GameAnimationSample(Sandbox Project) 기반 기술을 ExFrameWork 캐릭터 시스템에 적용

| 시스템 | 핵심 기술 | 문서 |
|--------|----------|------|
| **Visual Override** | Container Pawn + Visual Override Actor 분리 구조, 런타임 캐릭터 비주얼 교체, `UExVisualOverrideComponent` | `ExCore/Components/` |
| **Motion Matching** | `PoseSearchDatabase` + `ChooserTable` 기반 동적 애니메이션 선택, `CharacterTrajectoryComponent` 미래 경로 예측, Rewind Debugger 활용 | `Guides/Common/MotionMatching_Guide_KR.md` |
| **Motion Warping** | 루트 모션을 목표 지점에 정확히 맞추는 절차적 보정, Traversal(담넘기/오르기) 시 아티스트 키링 없이 장애물 위치에 자동 정렬 | `ExCore/ExCoreRuntime.Build.cs` |
| **Mover 2.0 캐릭터** | `UMoverComponent` 기반 캐릭터 이동, `ProduceInput` 패턴으로 입력 주입, 클라이언트 예측 + 서버 권한 보정 | `Architecture/ExCore/Mover_System_Analysis.md` |
| **UMG Video Background** | MediaPlayer 기반 루핑 동영상 메뉴, UI Material (Sampler Type: `Color`), 모바일 H.264 디코딩 | `Guides/Common/UMG_Video_Background_Setup_Guide.md` |

**GameAnimationSample 기반 적용 기술:**
- **Pose Search (Motion Matching)** — State Machine 대체, 수천 개 포즈에서 최적 프레임 실시간 검색
- **Chooser Table** — 게임플레이 태그/상태에 따른 동적 Database 전환
- **Character Trajectory** — 미래 이동 경로 예측으로 Motion Matching 검색 품질 향상
- **Motion Warping** — 루트 모션의 절차적 보정 (스트라이드/오리엔테이션 워핑, 타겟 위치 정렬)
- **Traversal System** — 장애물 감지(Trace) + Motion Matching 쿼리로 파쿠르 동작 자동 선택
- **Container/VisualOverride** — 이동/물리 담당 Pawn과 비주얼 Actor 분리, 런타임 교체

### Tooling

| 도구 | 설명 | 경로 |
|------|------|------|
| **Unreal Python Bridge (MCP Server)** | 외부 AI 에이전트가 실행 중인 언리얼 에디터에 TCP(9999) 접속하여 Python 코드를 실행하는 MCP 서버. 에셋 감사, 비주얼 분석, 자동화 작업 지원. 메인 스레드 안전 실행 보장 (`SlatePostTick` 큐잉) | `Tools/MCP/lyra_bridge_mcp.py` |
| **에디터 분석 스크립트** | ABP 분석, 캐릭터 모델 분석, 에셋 의존성 조회, GameMode 분석, 블루프린트 인스펙션 등 16개 전용 스크립트 | `Tools/MCP/*.py` |
| **Python Bridge 문서** | MCP 서버 구성, TCP 통신 프로토콜, CLI 사용법, 트러블슈팅 | `Guides/Common/PythonBridge_Documentation.md` |

---

## Branch Strategy

```
main ─────────────────────────────────────── 안정 릴리스 (프로덕션)
  │
  ├── RunnerV2 ──── 초기 러너 프로토타입 (아카이브)
  ├── RunnerV3 ──── 커브드 바닥 + 장애물 시스템 (아카이브)
  ├── RunnerV4 ──── DataCenter 통합 + UI 시스템 + Android 빌드 안정화
  └── RunnerV5 ★ ── 멀티플레이어 안정화 + 입력 모드 전략 + BGM 시스템 (현재 개발)
```

| 브랜치 | 상태 | 주요 작업 |
|--------|------|----------|
| `main` | 안정 | 프로덕션 기준 릴리스 |
| `RunnerV4` | 완료 | DataCenter 3-Base 체계, CommonUI 전환, ASTC 텍스처 쿠킹, Android Vulkan 안정화 |
| `RunnerV5` | **활성** | Mover 2.0 멀티플레이어, 입력 Strategy Pattern, Quartz BGM, GameRule 시스템 |

---

## UE5 Technical Highlights

- **GameFeatures Plugin Architecture** — Modular Gameplay 기반 Core/Feature 완전 분리
- **Mover 2.0** — 클라이언트 예측 프레임워크 기반 네트워크 이동 동기화
- **CommonUI** — 크로스 플랫폼 입력 라우팅, ActivatableWidget 스택 관리, 게임패드/터치 자동 전환
- **MVVM (Model-View-ViewModel)** — `UMVVMViewModelBase` + ViewBinding, UI와 로직 완전 분리
- **Enhanced Input** — `UInputAction` + `UInputMappingContext`, `InjectInputForAction`을 통한 UI→게임플레이 입력 주입
- **Quartz + MetaSounds** — 비트 동기화 오디오 엔진, Phase별 레이어 볼륨 실시간 제어
- **DataAsset 기반 데이터 드리븐** — Config/Definition/Preset 3-Base 체계, `GameInstance Subsystem`에서 중앙 관리
- **모바일 크로스 플랫폼** — Android(Vulkan/ASTC) 우선 개발, iOS 적용 예정. Zen DDC 캐시 관리, Cook-time INI 분리, 모바일 셰이더/텍스처 최적화
- **GameplayTag 이벤트 시스템** — 모듈 간 느슨한 결합, `FExGameplayEventPayload` 범용 페이로드
- **Motion Matching** — Pose Search Database + Chooser Table 기반 동적 애니메이션 선택, `CharacterTrajectoryComponent` 미래 경로 예측
- **Motion Warping** — 루트 모션의 절차적 보정 (Stride/Orientation Warping), Traversal 타겟 정렬
- **Visual Override System** — Container Pawn + Visual Override Actor 분리, 런타임 캐릭터 비주얼 교체
- **Unreal MCP Server** — 자체 제작 Python Bridge MCP 서버로 AI 에이전트가 에디터 내부 Python API에 직접 접근. 에셋 분석/자동화 지원

---

## Development Guidelines

프로젝트 규칙 및 코딩 컨벤션은 [`Md/ExFrameWork_Guidelines.md`](Md/ExFrameWork_Guidelines.md)를 참조하세요.

주요 규칙:
- `Ex` 접두사 네이밍 (`UExWidget`, `AExActor`, `DA_Ex[Name]`)
- `LogExFrameWork` 전용 로그 카테고리 사용 (`LogTemp` 금지)
- `TObjectPtr` + `UPROPERTY()` 조합 (TWeakObjectPtr는 UPROPERTY 없이 사용)
- `CreateWeakLambda` 필수 (Native Delegate 바인딩 시)
- 서버 권한(Authority) 분리 고려 필수
- Architecture 문서 선행 → 구현 (Report & Approval 프로세스)
