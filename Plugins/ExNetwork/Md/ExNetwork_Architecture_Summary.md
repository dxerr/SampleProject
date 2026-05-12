# ExNetwork — 아키텍처 요약

> **플러그인 정체성:** ExFrameWork 프로젝트의 매칭/온라인 통합 모듈. 어디서든 공통으로 사용 가능한 독립 플러그인.
> **라이선스/이식성:** 다른 UE 프로젝트로 그대로 복사하여 재사용 가능한 수준의 독립성 추구.
> **현재 단계:** Phase 1 — 골격 + EOS SDK 통합 (v2 정책 적용 중)

---

## 존재 이유 (Why This Plugin)

본 플러그인은 다음 세 가지 핵심 가치를 제공한다.

1. **백엔드 추상화** — EOS, Steam, PSN, Custom 등 어떤 온라인 백엔드를 쓰더라도 게임 코드는 변경하지 않는다. Provider Pattern으로 백엔드를 교체 가능하게 한다.

2. **서버 모델 통합** — Listen Server와 Dedicated Server를 동일 인터페이스로 다룬다. Strategy Pattern으로 환경에 따라 자동 선택되며, 게임 코드는 어떤 모델인지 알 필요가 없다.

3. **모드 무관 공통 모듈** — Runner, Battle, RPG 등 어떤 게임 모드에서도 동일한 매칭 시스템을 활용할 수 있다. 각 GameFeature는 자신만의 매칭 정책만 오버라이드한다.


---

## 핵심 설계 원칙 (Core Principles)

### 1. Provider Pattern — 백엔드 교체 가능성

```
IExAuthProvider         ← 인증 추상화 (Device/Epic/Custom)
IExLobbyProvider        ← 로비 추상화 (EOS Lobby/Steam Lobby)
IExMatchmakingProvider  ← 매칭 추상화 (EOS Matchmaking/Skill-based)
```

각 인터페이스는 EOS, Null(테스트용), 향후 Steam 등 다양한 구현체를 가진다. 게임 코드는 인터페이스만 참조하므로 백엔드 교체가 자유롭다.

### 2. Strategy Pattern — 서버 모델 추상화

```
IExNetServerStrategy
  ├─ ExListenServerStrategy     (호스트 PC가 서버 역할)
  └─ ExDedicatedServerStrategy  (별도 서버 인스턴스)
```

`UExOnlineSubsystem`이 환경(빌드 타깃, NetMode)을 감지하여 적절한 Strategy를 자동 선택한다. 게임 코드는 Strategy를 통해 매치를 생성/참가/시작하므로 환경 의존성이 사라진다.

### 3. GameFeature 오버라이드 가능성

각 GameFeature(`ExRunnerPlay`, 향후 `ExBattlePlay` 등)는 다음을 자유롭게 정의한다.

- 매칭 파라미터 (`FExMatchConfig` 기반 DataAsset)
- 매칭 정책 오버라이드 (인원 수, 검색 속성, 카운트다운 등)
- Lobby UI 연출
- 게임 진입 후 단계별 흐름 (기존 `Match_WaitingForPlayers` 등 활용)

ExNetwork 자체는 **모드를 모르며**, 모드별 정책은 외부에서 주입된다.


### 4. 코드 레이아웃 정책 (ExFrameWork 통일)

본 플러그인은 ExFrameWork의 표준 정책을 따라 다음을 준수한다.

- **Public/Private 폴더 분리 사용하지 않음** — `.h`와 `.cpp` 파일을 같은 위치에 평면 배치
- 외부 모듈의 헤더 참조는 `Build.cs`의 `PublicIncludePaths`에 서브폴더를 명시적으로 등록하여 해결
- 모듈명은 `ExNetworkRuntime` (ExCore의 `ExCoreRuntime` 패턴 일치)
- 책임별 서브폴더 분할 (`Core/`, `Strategies/`, `Providers/` 등)

이 정책은 프로젝트 전체 코드베이스의 일관성을 유지하며, 외부 협업자에게 혼선을 주지 않기 위한 명시적 규약이다.

---

## 시스템 다이어그램 (Conceptual)

```
┌──────────────────────────────────────────────────────────────┐
│  Game Code (ExRunnerPlay, ExBattlePlay, ...)                 │
│  ─ Uses: UExOnlineSubsystem 진입점                            │
│  ─ Defines: FExMatchConfig (DataAsset)                       │
└──────────────────────────────────────────────────────────────┘
                          ↓ (uses)
┌──────────────────────────────────────────────────────────────┐
│  ExNetwork Plugin                                            │
│                                                              │
│  UExOnlineSubsystem (GameInstanceSubsystem, 진입점)           │
│    ├─ AuthProvider     : IExAuthProvider                     │
│    ├─ LobbyProvider    : IExLobbyProvider                    │
│    ├─ MMProvider       : IExMatchmakingProvider              │
│    └─ ServerStrategy   : IExNetServerStrategy                │
│                                                              │
│  Providers/EOS/                                              │
│    ├─ ExEOSAuthProvider                                      │
│    ├─ ExEOSLobbyProvider                                     │
│    └─ ExEOSMatchmakingProvider                               │
│                                                              │
│  Strategies/                                                 │
│    ├─ ExListenServerStrategy                                 │
│    └─ ExDedicatedServerStrategy                              │
└──────────────────────────────────────────────────────────────┘
                          ↓ (depends on)
┌──────────────────────────────────────────────────────────────┐
│  UE OnlineSubsystem (엔진 표준 추상화 레이어)                  │
└──────────────────────────────────────────────────────────────┘
                          ↓ (implemented by)
┌──────────────────────────────────────────────────────────────┐
│  Redpoint EOS Online Framework (외부 플러그인)                │
└──────────────────────────────────────────────────────────────┘
                          ↓ (wraps)
┌──────────────────────────────────────────────────────────────┐
│  Epic Online Services SDK (백엔드)                            │
└──────────────────────────────────────────────────────────────┘
```


---

## 외부 인터페이스 (Public Surface)

다른 모듈이 본 플러그인을 사용하는 방법은 단순하다. **모든 진입은 `UExOnlineSubsystem` 단일 게이트로 수렴한다.**

```
GameInstance->GetSubsystem<UExOnlineSubsystem>()
  ├─ Login()              // Phase 2
  ├─ FindQuickMatch()     // Phase 3
  ├─ HostMatch(Config)    // Phase 3
  ├─ JoinMatch(Handle)    // Phase 3
  ├─ StartGameSession()   // Phase 4
  └─ DestroyMatch()       // Phase 4
```

Phase 1에서는 `UExOnlineSubsystem` 골격만 존재하며, 위 함수들은 후속 Phase에서 점진적으로 채워진다.

---

## 의존성 방향 (Dependency Direction)

```
[ExRunnerPlay, ExBattlePlay, ...] ──→ [ExNetwork] ──→ [ExCore] (옵션)
                                             │
                                             └─→ [OnlineSubsystem (UE)]
                                                   └─→ [Redpoint EOS]
                                                         └─→ [EOS SDK]
```

핵심 원칙:

- ExCore는 ExNetwork를 **모른다** — 매칭 없이 싱글플레이만 가능
- 모든 GameFeature는 ExNetwork를 사용할 수도, 사용하지 않을 수도 있다
- ExNetwork는 어떤 게임 모드도 모른다 — 모드 정책은 외부에서 주입
- ExNetwork는 EOS에 강결합되지 않는다 — Provider Pattern으로 백엔드 교체 가능


---

## 단계별 빌드업 (Phased Buildup)

본 플러그인은 단번에 완성되지 않으며 6개 Phase로 점진 구축된다. 각 Phase는 독립 Plan 문서로 관리된다.

| Phase | 책임 | 상태 |
|---|---|---|
| 1 | 플러그인 골격 + Redpoint 활성화 | 현재 진행 (v2) |
| 2 | 인증 + 인터페이스 코어 + Server Strategy + DefaultPlatformService 전환 | 예정 |
| 3 | Lobby Provider + Quick Match 매칭 + NetDriverDefinitions 등록 | 예정 |
| 4 | Lobby → Game 전환 + 기존 흐름 연결 | 예정 |
| 5 | Matchmaking UI | 예정 |
| 6+ | 매칭 방식 확장 (Invite/Friend/Skill) | 장기 |

각 Phase는 외부 AI 에이전트 피드백을 거쳐 검증된다.

---

## 관련 문서

- 모듈 폴더 구조 상세: [`ExNetwork_Module_Layout.md`](./ExNetwork_Module_Layout.md)
- Phase 1 Plan (구현 명세, v2): [`../../../Md/Plans/ExNetwork_Phase1_Plugin_Skeleton_Plan.md`](../../../Md/Plans/ExNetwork_Phase1_Plugin_Skeleton_Plan.md)
- 코딩 규칙 및 프로젝트 가이드라인: [`../../../Md/ExFrameWork_Guidelines.md`](../../../Md/ExFrameWork_Guidelines.md)
