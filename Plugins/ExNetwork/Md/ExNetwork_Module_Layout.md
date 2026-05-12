# ExNetwork — 모듈 폴더 구조 (Module Layout)

> **문서 목적:** 본 플러그인의 폴더 구조와 각 폴더의 책임을 한 페이지로 정리.
> **대상 독자:** 본 플러그인을 수정/확장/이식하려는 개발자.
> **변경 시점:** 새 Phase 진행 시 폴더가 추가되면 본 문서도 갱신.
> **정책 버전:** v2 — ExFrameWork 표준 정책 (Public/Private 분리 없음) 반영

---

## ExFrameWork 코드 레이아웃 정책 (필독)

본 플러그인은 ExFrameWork 프로젝트 전체 정책을 따라 다음을 준수한다.

- **`.h`와 `.cpp` 파일을 같은 위치에 평면 배치**
- **`Public/`, `Private/` 폴더 분리 사용하지 않음**
- 외부 모듈의 헤더 참조는 `Build.cs`의 `PublicIncludePaths`에 서브폴더를 명시적으로 등록하여 해결
- 모듈 루트(`ExNetworkRuntime/`)에는 Module 클래스(`ExNetworkRuntimeModule.h/cpp`)와 Build.cs만 평면 배치
- 책임별 서브폴더(`Core/`, `Strategies/` 등)에는 해당 책임의 `.h`/`.cpp`가 함께 배치

이 정책은 ExCore(`ExCoreRuntime`)의 패턴을 그대로 따른 것이며, 프로젝트 전체 일관성 유지를 위한 명시적 규약이다. 외부 협업자가 본 플러그인을 다룰 때 혼선을 주지 않기 위해 본 문서 최상단에 명시한다.


---

## 최상위 구조

```
Plugins/ExNetwork/
├── ExNetwork.uplugin            ← 플러그인 매니페스트 (FriendlyName, Modules 등)
├── Resources/
│   └── Icon128.png              ← Editor 표시용 아이콘
├── Config/
│   └── FilterPlugin.ini         ← 패키징 시 포함/제외 파일 필터
├── Md/                          ← 본 문서 위치 (아키텍처 문서)
│   ├── ExNetwork_Architecture_Summary.md
│   └── ExNetwork_Module_Layout.md  (이 문서)
└── Source/
    └── ExNetworkRuntime/        ← 단일 런타임 모듈 (Phase 1 기준)
```

향후 필요 시 `ExNetworkEditor` 모듈을 추가하여 에디터 도구를 분리할 수 있다 (Phase 5 이후).

---

## ExNetworkRuntime 모듈 내부 구조 (전체 비전)

본 단계(Phase 1)에서는 `Core/`만 채워지지만, 후속 Phase에서 다음 구조로 확장된다.

```
Source/ExNetworkRuntime/
├── ExNetworkRuntime.Build.cs          ← 모듈 빌드 의존성
├── ExNetworkRuntimeModule.h           ← 모듈 클래스 헤더
├── ExNetworkRuntimeModule.cpp         ← 모듈 진입점 (IMPLEMENT_MODULE)
│
├── Core/                              ← Phase 1: 진입점과 공통 유틸
│   ├── ExOnlineSubsystem.h            ← UGameInstanceSubsystem (진입점)
│   ├── ExOnlineSubsystem.cpp
│   ├── ExNetworkLog.h                 ← 로그 카테고리 선언
│   └── IExNetServerStrategy.h         ← Phase 2: 서버 모델 인터페이스
│
├── Strategies/                        ← Phase 2: 서버 모델 구현
│   ├── ExListenServerStrategy.h
│   ├── ExListenServerStrategy.cpp
│   ├── ExDedicatedServerStrategy.h
│   └── ExDedicatedServerStrategy.cpp
│
├── Providers/                         ← Phase 2~3: 백엔드 구현
│   ├── IExAuthProvider.h              ← Phase 2: 인증 인터페이스
│   ├── IExLobbyProvider.h             ← Phase 3: 로비 인터페이스
│   ├── IExMatchmakingProvider.h       ← Phase 6+: 매칭 인터페이스
│   │
│   ├── EOS/                           ← EOS 구현체
│   │   ├── ExEOSAuthProvider.h/cpp
│   │   ├── ExEOSLobbyProvider.h/cpp
│   │   └── ExEOSMatchmakingProvider.h/cpp
│   │
│   └── Null/                          ← 테스트/오프라인 구현체
│       └── ExNullProviders.h/cpp
│
├── Match/                             ← Phase 3: 매칭 데이터 모델
│   ├── ExMatchConfig.h                ← USTRUCT (모드별 매칭 설정)
│   ├── ExMatchHandle.h                ← 매치 식별자
│   └── ExMatchAttributes.h            ← 검색 속성
│
├── Player/                            ← Phase 3: 플레이어 모델
│   └── ExNetPlayerProfile.h
│
└── Events/                            ← Phase 2: 델리게이트 정의
    └── ExNetEvents.h
```

**핵심 사항:** 모든 폴더 내에서 `.h`와 `.cpp`가 같은 위치에 배치되며, `Public/Private` 폴더는 어디에도 사용되지 않는다.


---

## 폴더별 책임 정의

| 폴더 | 책임 | 들어가는 것 | 들어가지 않는 것 |
|---|---|---|---|
| `Core/` | 진입점, 공통 유틸, 인터페이스 일부 | Subsystem, 로그, 공통 인터페이스 | 백엔드 구현, 매칭 로직 |
| `Strategies/` | 서버 모델별 구현 | Listen/Dedicated Strategy | 백엔드 의존 코드 |
| `Providers/` | 백엔드별 구현 | EOS, Null, 향후 Steam 등 | 서버 모델 구분 코드 |
| `Match/` | 매칭 데이터 모델 | USTRUCT, DataAsset 정의 | 매칭 실행 로직 (Provider 책임) |
| `Player/` | 플레이어 식별/프로필 | NetID, DisplayName 등 USTRUCT | PlayerState (게임 코드 책임) |
| `Events/` | 델리게이트 정의 | OnLogin, OnMatchFound 등 | 핸들러 구현 |

이 책임 분리는 가이드라인 1.4(단일 책임) 및 1.5(폴더 구조)를 따른다.

---

## Build.cs의 PublicIncludePaths 등록 패턴

ExFrameWork 정책상 Public/Private 분리를 사용하지 않으므로, 외부 모듈이 본 플러그인의 헤더를 짧은 경로로 참조하려면 `ExNetworkRuntime.Build.cs`에 각 서브폴더를 `PublicIncludePaths`로 등록해야 한다.

**Phase 1에서 등록되는 경로:**
- `ModuleDirectory` (루트 — Module 헤더 접근용)
- `Path.Combine(ModuleDirectory, "Core")`

**Phase 2 이후 추가될 경로:**
- `Strategies/`, `Providers/`, `Providers/EOS/`, `Providers/Null/`, `Events/` — Plan 2 시점
- `Match/`, `Player/` — Plan 3 시점

이 패턴은 ExCore의 `ExCoreRuntime.Build.cs`에서 동일하게 적용되어 있으므로 참조하면 된다.


---

## 명명 규칙 (Naming Conventions)

가이드라인 1.2를 본 플러그인 컨텍스트로 구체화:

| 종류 | 접두사 | 예시 |
|---|---|---|
| Subsystem 클래스 | `UEx` | `UExOnlineSubsystem` |
| 인터페이스 | `IEx` | `IExAuthProvider` |
| 인터페이스 구현체 | `UEx` 또는 `FEx` | `UExEOSAuthProvider` |
| USTRUCT | `FEx` | `FExMatchConfig` |
| Enum | `EEx` | `EExMatchState` |
| 델리게이트 | `FEx...Signature` | `FExOnLoginCompleteSignature` |
| 로그 카테고리 | `LogEx` | `LogExNetwork` |
| 매크로(public API) | `EXNETWORKRUNTIME_API` | 모든 export 클래스/함수 |
| 모듈 클래스 | `FEx...Module` | `FExNetworkRuntimeModule` |

---

## Phase별 채워질 영역 예고

각 Phase가 진행되며 어떤 폴더가 채워지는지 한눈에 정리.

| Phase | 신규 채워지는 폴더 | 비고 |
|---|---|---|
| **1 (현재)** | `Core/` (Subsystem 골격, 로그) | Redpoint 활성화만 (OSS 기본값 전환은 Phase 2) |
| **2** | `Core/` (서버 인터페이스), `Strategies/`, `Providers/EOS/AuthProvider`, `Providers/Null/`, `Events/` | 인증 동작 + DefaultPlatformService 전환 |
| **3** | `Match/`, `Player/`, `Providers/EOS/LobbyProvider` | Quick Match 동작 + NetDriverDefinitions 등록 |
| **4** | (기존 폴더 보강 위주) | Lobby → Game 전환 |
| **5** | (외부 UI 모듈, 본 플러그인 외부) | UI 작업 |
| **6+** | `Providers/EOS/MatchmakingProvider` 등 | 매칭 방식 확장 |


---

## 의존성 정책 (Dependency Policy)

본 플러그인이 의존할 수 있는 대상:

- ✅ UE 엔진 모듈 (`Core`, `CoreUObject`, `Engine`) — Phase 1
- ✅ UE OnlineSubsystem (`OnlineSubsystem`, `OnlineSubsystemUtils`) — Phase 2 이후
- ✅ Redpoint EOS Online Framework (외부 플러그인) — Phase 1 활성화, Phase 2 본격 사용
- ⚠️ ExCore (옵션 — 필요 시점에 추가 검토, 기본 미의존 권장)
- ❌ 어떤 GameFeature도 직접 의존 금지 (ExRunnerPlay, ExBattlePlay 등)
- ❌ 특정 게임 모드 클래스(`AExRunnerGameMode` 등) 직접 참조 금지

본 플러그인을 의존하는 대상:

- ✅ 모든 GameFeature (ExRunnerPlay 등) — 필요 시
- ✅ 게임 UI 모듈
- ❌ ExCore는 본 플러그인을 모름 (역방향 의존 금지)

---

## 이식 가이드 (Portability Notes)

본 플러그인을 다른 UE 프로젝트로 이식하려면 다음만 수행하면 된다.

1. `Plugins/ExNetwork/` 폴더 전체를 대상 프로젝트의 `Plugins/`로 복사
2. Redpoint EOS Online Framework 플러그인을 대상 프로젝트에 설치
3. 대상 프로젝트의 `DefaultEngine.ini`에 EOS Subsystem 설정 추가 (Phase 2 이후 설정)
4. 대상 프로젝트의 EOS Developer Portal Product 정보 입력
5. 대상 프로젝트의 `.uproject`에 본 플러그인 활성화 항목 추가

본 플러그인은 ExFrameWork의 어떤 코드도 강제 참조하지 않으므로 단독 이식이 가능하다. ExCore 의존성이 추후 추가될 경우 이식 가이드도 갱신된다.

**ExFrameWork 정책 이식 시 주의:** Public/Private 미사용 정책은 ExFrameWork 고유 규약이다. 다른 프로젝트로 이식할 때 해당 프로젝트가 표준 UE 패턴(Public/Private 분리)을 따르고 있다면, 본 플러그인의 폴더 구조를 그 프로젝트 정책에 맞게 재배치하거나 본 정책을 그대로 유지하기로 결정해야 한다.

---

## 관련 문서

- 아키텍처 요약: [`ExNetwork_Architecture_Summary.md`](./ExNetwork_Architecture_Summary.md)
- Phase 1 Plan (v2): [`../../../Md/Plans/ExNetwork_Phase1_Plugin_Skeleton_Plan.md`](../../../Md/Plans/ExNetwork_Phase1_Plugin_Skeleton_Plan.md)
- 프로젝트 가이드라인: [`../../../Md/ExFrameWork_Guidelines.md`](../../../Md/ExFrameWork_Guidelines.md)
