# ExNetwork — Match State Machine 도입 Plan

> **버전:** v1.2 (외부 AI 피드백 검토 반영본)
> **목적:** 현재 `UExOnlineSubsystem`의 매칭 흐름 처리에 누적된 if-가드 패턴(암묵적 상태 머신)을 명시적 FSM 구조로 재편하여, 상태 전이의 안전성·디버깅 가능성·확장성을 확보한다.
> **상태:** 설계 완료 — 구현 승인 대기
> **선행 완료:** ExNetwork Phase 1 ~ Phase 4 (Plugin Skeleton / Auth / Lobby QuickMatch / Lobby→Game 전환)
> **관련 문서:**
> - `Md/Architecture/ExCore/ExFrameWork_Multiplayer_Flow_Architecture.md` v2.0 — Flow/Match 상태 계층 정의
> - `Md/Bug/MultiPlay/Bug_Mover_Multiplayer_Sync_Error.md` — 분산된 상태 갱신으로 인한 동기화 부채 사례
> **후속 작업:**
> - `ExFrameWork_Multiplayer_Flow_Architecture.md` v2.1 갱신 — Flow.Lobby sub-state 명문화
> - `MatchUI_FSM_Integration_Plan` (가칭) — UI ViewModel의 자체 상태 플래그를 `OnMatchStateChanged` 구독 기반으로 일원화
> - Dedicated Server 전략의 FSM 적용 — 본 Plan에서 정비된 `IExNetServerStrategy` 인터페이스를 그대로 구현
>
> **v1.2 변경 요약:** § 6.4 / § 11.2 (Network Travel Failure 예외 처리 및 Reset Transient State 파괴 보장, 명명 부채 기록)
> **v1.1 변경 요약:** § 1 / § 4 / § 5.3 / § 6.1 / § 6.2 / § 7.1 / § 7.2 / § 8.1 / § 11.2 / § 11.3 / § 14(신규)
> 자세한 개정 내역은 § 14 "Revision History" 참조.

---

## 1. 작업 범위 (Scope)

### 1.1 본 Plan에서 진행하는 것

- `Plugins/ExNetwork/Source/ExNetworkRuntime/Match/ExMatchTypes.h`
  - `EExMatchState` enum 정리 (각 값의 의도 명확화 — § 4.1 참조)
  - 상태 전이 메타데이터 정의 (전이 맵에 사용)
- `Plugins/ExNetwork/Source/ExNetworkRuntime/Core/IExNetServerStrategy.h` **(v1.1 신규 항목)**
  - 인터페이스 보강 — Phase 메서드 군(`BeginSearchPhase / EndSearchPhase / ...`)을 베이스 가상 함수로 선언 (Listen-only 메서드의 캐스팅 분산을 구조적으로 해소)
  - 베이스 인터페이스에 Listen/Dedicated 공통 조회용 가상 함수 추가: `IsHost()`, `GetConnectString()`, `CancelMatch()`, `ResetTransientState()`
  - Dedicated 구현은 본 Plan에서는 빈 구현(no-op) 또는 `ensureMsgf("Not implemented in DedicatedStrategy")` 형태로 둔다. 실제 동작은 후속 Dedicated FSM Plan에서 채운다
  - 본 항목의 목적은 **Dedicated 완전 지원이 아니라**, `ExOnlineSubsystem.cpp`에서 `static_cast<FExListenServerStrategy*>` 패턴이 5곳에 흩어지는 구조적 부채를 본 Plan 단계에서 함께 청산하는 것
- `Plugins/ExNetwork/Source/ExNetworkRuntime/Core/ExOnlineSubsystem.h/cpp`
  - **단일 전이 진입점** `TransitionMatchState(NewState, Reason)` 도입 — Reason 인자는 v1.1에서 추가 (안전 우선 정책 § 5.3)
  - **Enter/Exit 디스패치** (`HandleEnterMatchState` / `HandleExitMatchState`)
  - 상태 전이 유효성 검증 (Transition Map)
  - 외부 알림 델리게이트 추가 (`OnMatchStateChanged`)
  - 외부 공개 API (`FindQuickMatch / CancelMatch / StartGame / ResetMatchState`) **시그니처는 보존**, 내부 흐름만 재편
  - 비동기 콜백 경계 재진입 가드 (re-entrancy guard)
  - **(v1.1)** Strategy 접근 시 `static_cast<FExListenServerStrategy*>` 패턴 제거 — 베이스 인터페이스 `IExNetServerStrategy*` 만으로 매칭 흐름 전체 수행. Listen 전용 동작이 필요한 1~2곳(예: LobbyProvider 주입 등 초기화 경로)에 한해 캐스팅 허용하되 그 위치를 명시적으로 한정
- `Plugins/ExNetwork/Source/ExNetworkRuntime/Strategies/ExListenServerStrategy.h/cpp`
  - 책임 경계 재정의 — Strategy는 **무엇을 할지(작업 실행자)** 만 담당, **언제 할지(상태)** 는 Subsystem 소관
  - `FindAndJoinOrCreate` 내부의 단계 로직을 의도 기반 진입 메서드 단위로 분리 (`BeginSearchPhase / BeginCreatePhase / BeginWaitPhase` 형태의 책임 단위)
  - Strategy 내부의 `bIsHost`, `WaitStartTime`, `CachedConnectString` 등 휘발성 상태값은 그대로 유지하되, 상태 전환 트리거는 Subsystem에서 호출
- 디버그 / Cheat 지원 — 가이드라인 1.11 준수
  - 임의 상태 강제 진입을 위한 디버그 전용 함수 (개발 빌드에서만 활성화)
  - 상태 전이 로그를 일관된 포맷으로 출력

### 1.2 본 Plan에서 진행하지 않는 것

- `UExGameFlowSubsystem` 자체의 구현/수정 (별도 Plan: `ExGameFlow_Transition_Plan.md` 영역)
- `AExGameStateBase`가 관할하는 **Match.\* GameplayTag 계층** 변경 (인게임 매치 상태는 본 Plan의 범위 외)
- **UI ViewModel 통합 (v1.1 명시화)** — 매치메이킹 위젯(`UExLobbyMatchViewModel`)이 보유한 자체 상태 플래그(`bIsMatching`, `bPendingStartMultiPlay` 등) 및 `ResetMatchState()` 강제 호출 패턴의 일원화 작업. 본 Plan 완료 후 후속 작업 `MatchUI_FSM_Integration_Plan` (가칭)에서 `OnMatchStateChanged` 구독 기반으로 단일 진실 소스(Single Source of Truth)로 통합한다. 본 Plan 단계에서는 `OnMatchStateChanged` 델리게이트만 제공하고, UI 측 구독 작업은 의도적으로 분리한다 (Plan 비대화 방지)
- Dedicated Server 전략(`FExDedicatedServerStrategy`)의 **실제 매칭 로직 구현** — 본 Plan은 Listen Server 흐름을 우선 정리하며, Dedicated 흐름은 동일 패턴을 따르되 별도 Plan에서 진행. **단, 베이스 인터페이스(`IExNetServerStrategy`) 보강은 본 Plan 범위에 포함됨** (§ 1.1 참조)
- `EExMatchState` 자체를 GameplayTag 기반으로 치환하는 작업 — 본 Plan에서는 enum 형태를 유지하며 FSM 구조만 도입 (단순함 우선)

---

## 2. 문제 정의 (Problem Statement)

### 2.1 현재 구조의 문제 — 암묵적 상태 머신

`UExOnlineSubsystem`은 `EExMatchState CurrentMatchState` 멤버 변수 하나로 매칭 흐름을 추적하지만, 실제로는 다음과 같은 분산된 패턴을 보인다:

1. **상태 검증이 함수마다 if 가드로 산재한다.** `FindQuickMatch`는 `Idle` 인지 검사하고, `StartGame`은 `Ready` 인지 검사하며, `CancelMatch`는 `InGame` 이면 무시한다. 향후 상태(예: `Reconnecting`, `Disconnected`)가 추가되면 각 함수에 if 가지가 추가로 늘어난다.
2. **상태 변경이 분산되어 있다.** `CurrentMatchState = X;` 형태의 직접 대입이 다섯 군데 이상에 흩어져 있어, 추적과 디버깅이 어렵다.
3. **상태 진입/진출 시 해야 할 작업이 코드 본문에 묻혀 있다.** `Searching` 진입 시 람다 바인딩·시간 캡처·Strategy 호출이 `FindAndJoinOrCreate` 내부에 섞여 있고, 진출 시 정리 작업은 `ClearWaitLobbyTicker`/`OnFindComplete.Clear()` 등으로 흩어져 있다.
4. **전이 유효성이 강제되지 않는다.** `Idle → InGame` 같은 불가능한 점프가 발생해도 컴파일 타임/런타임 어디서도 막을 방법이 없다. 이는 가이드라인 1.7 "Silent Failure 방지" 원칙과 배치된다.
5. **외부 알림 누락 가능성.** 상태가 바뀌어도 외부에 알리는 표준 채널이 없다. 현재는 `OnMatchFound`/`OnGameStarted` 결과 델리게이트만 있고, **상태 변화 자체**를 구독할 통로가 없다.

### 2.2 멀티플레이 환경에서의 가중치

이미 `Bug/MultiPlay/Bug_Mover_Multiplayer_Sync_Error.md`에 기록된 교훈이 동일 패턴을 반복하지 말 것을 시사한다 — *"상태 전환 시점에 일관된 의도를 명시적으로 전달하지 않으면 분산된 위치에서 동기화 누락이 발생한다."*

또한 현재 코드에는 비동기 콜백 안전성 관련 주석이 다수 존재한다:
- *"검색 재시도 Ticker 해제 — 이것을 빠뜨리면 소멸 후 람다가 실행되어 댕글링 포인터 크래시 발생"*
- *"EOS SDK 콜백 스택 내부에서 DestroySession이나 ServerTravel이 동기적으로 실행되는 것을 방지하기 위해 다음 틱으로 지연"*
- *"DestroyLobby는 여기서 직접 호출하지 않음 — 이중 파괴(double destroy)가 발생하여 MatchMode 문자열 오염의 원인이 됨"*

이는 모두 **상태 진출 타이밍이 정의되어 있지 않아 후속 정리 작업이 산발적으로 코드에 흩어진 결과**이며, 명시적 FSM 도입으로 구조적으로 해결되어야 할 부채다.

### 2.3 ExFrameWork 컨텍스트 — 상태 계층의 정확한 위치

`Md/Architecture/ExCore/ExFrameWork_Multiplayer_Flow_Architecture.md` v2.0은 이미 두 가지 상태 계층을 정의한다:

| 계층 | 관할 | 상태 예시 |
|---|---|---|
| **Flow (앱 레벨)** | `UExGameFlowSubsystem` | `Flow.Boot / Flow.Auth.IDP / Flow.Lobby / Flow.InGame` |
| **Match (맵 내부 매치)** | `AExGameStateBase` | `Match.WaitingForPlayers / Match.Countdown / Match.Playing / Match.PostMatch` |

`EExMatchState`는 이 두 계층 어디에도 속하지 않는 **제3의 상태**다. 정확히는 **`Flow.Lobby` 안에서만 살아가는 매치메이킹 sub-state** 이며, "매치를 찾는 과정 자체"의 진행 상태이지 인게임 매치 상태가 아니다.

본 Plan은 이 sub-state 계층을 명문화하고, 동일 계층 안에서 안전한 전이를 보장하는 FSM 골격을 구축한다. 상위 Flow 계층(`UExGameFlowSubsystem`)과의 연동은 본 Plan에서는 다루지 않으며, 후속 문서(Multiplayer_Flow_Architecture.md v2.1)에서 명문화한다.

---

## 3. 핵심 설계 원칙

### 3.1 "엔진형 경량 FSM" 패턴 채택

전통적 GoF State Pattern (상태별 별도 클래스 + Enter/Exit 가상 함수)은 본 케이스에 과한 설계다. 다음 이유로 **경량 FSM 패턴**을 채택한다:

- 상태 수가 7개로 적다. 별도 클래스화 시 보일러플레이트가 본질 로직을 능가한다.
- 상태 간 공유 데이터가 적다. 상태별 멤버 변수 분리의 이득이 적다.
- 이미 Strategy 패턴(`IExNetServerStrategy`)이 적용되어 있다. State 패턴까지 겹치면 가상 디스패치 두 축이 교차하여 추적성이 악화된다.

대신 다음 세 가지 핵심 아이디어만 차용한다:
1. **단일 전이 진입점** — 모든 상태 변경은 한 함수를 통해서만.
2. **명시적 Enter/Exit 콜백** — `switch` 디스패치로 충분.
3. **전이 유효성 검증** — Transition Map + `ensureMsgf`.

### 3.2 외부 API 보존 원칙 (Backward Compatibility)

`FindQuickMatch / CancelMatch / StartGame / ResetMatchState / GetMatchState` 등 외부 공개 시그니처는 그대로 유지한다. BP 호출부와 외부 모듈(예: `ExRunnerPlay`)의 코드 수정을 강제하지 않는다. 내부 흐름만 FSM 기반으로 재편되며, 외부에서 보기에는 "동일한 함수가 더 안전하고 일관되게 동작한다"는 경험이 된다.

### 3.3 책임 경계 명확화 — Subsystem vs Strategy

| 책임 | 담당 | 비고 |
|---|---|---|
| 상태 보유·전이 결정·전이 유효성 검증 | `UExOnlineSubsystem` | 단일 책임 — "언제 무엇을 할지" |
| 상태 진입 시 호출되는 작업 실행 | `FExListenServerStrategy` | 단일 책임 — "무엇을 한다" |
| 외부 알림 (BP 델리게이트 브로드캐스트) | `UExOnlineSubsystem` | Strategy는 알림 채널 모름 |
| EOS API 호출, Lobby Provider 작업 | `FExListenServerStrategy` 내부 | Subsystem은 EOS 세부 모름 |

Strategy는 더 이상 "이 시점에 다음 상태로 가야 한다"를 결정하지 않는다. 대신 작업 완료 결과를 콜백/델리게이트로 Subsystem에 통보하고, Subsystem이 그 결과를 보고 전이를 결정한다. 이로써 Strategy는 **상태 무관 작업 실행자**가 되며, 향후 Dedicated 전략을 도입할 때도 동일 패턴이 적용된다.

### 3.4 비동기 콜백 재진입 안전성 (Re-entrancy Safety)

EOS SDK 콜백 스택 내부에서 동기적으로 `DestroySession`/`ServerTravel` 등을 실행하면 크래시 위험이 있음이 코드 주석에 이미 명시되어 있다. 본 FSM은 다음 보장을 내장한다:

- **전이 중 전이 호출 감지** — `TransitionMatchState`는 자기 자신이 진행 중인 동안 재호출되면 다음 틱으로 지연 실행한다.
- **Exit → Enter 순서 보장** — 새 상태 진입 작업은 이전 상태 진출 정리가 완료된 후에만 시작된다.
- **소멸 가드 통합** — `bIsDestroyed` 플래그 검사 패턴을 FSM 디스패치 진입부에서 일괄 수행하여, 개별 콜백마다 반복 검사하지 않게 한다.

### 3.5 디버깅·치트 친화성 (가이드라인 1.11)

- **상태 전이 일관 로그** — `[OldState] → [NewState]` 포맷 단일 출력 지점.
- **상태 강제 진입 디버그 함수** — `WITH_EDITOR` 또는 `!UE_BUILD_SHIPPING` 가드 하에 임의 상태 진입 허용. 본 핵심 비즈니스 로직과 분리된 함수로 제공.
- **상태 시각화 훅** — 외부 디버그 패널(예: 향후 ImGui/디버그 위젯)이 `OnMatchStateChanged` 델리게이트만 구독하면 즉시 상태 흐름을 추적 가능.

---

## 4. 상태 정의 (State Definition)

### 4.1 `EExMatchState` 상태별 의미 명확화

현행 enum의 각 상태에 대해, FSM 도입 후의 명확한 의미·진입 조건·진출 조건을 정의한다.

| 상태 | 의미 | 진입 트리거 | 진출 트리거 |
|---|---|---|---|
| `Idle` | 매칭 비활성 — 어떤 매칭 동작도 진행 중이지 않음. 초기 상태이자 정상 종료 상태. | Subsystem 초기화 / 정상 종료 / 취소 완료 / 실패 폴백 | `FindQuickMatch()` 외부 호출 |
| `Searching` | 같은 MatchMode의 기존 Lobby 검색 중. 재시도 루프 포함. | `FindQuickMatch()` 호출 (멀티플레이) | 검색 결과 발견 → `Joining` / 타임아웃 → `Creating` / 취소 → `Idle` |
| `Creating` | 자신이 Host가 되어 Lobby를 생성하는 중. | 검색 결과 없음 + 대기 시간 만료 / SinglePlay 즉시 진입 | 생성 성공 → `Waiting` / 실패 → `Idle` |
| `Waiting` | Lobby 생성 완료, 다른 플레이어 입장 대기 중. (Host 관점) | `Creating` 성공 | 정원 충족 → `Ready` / 타임아웃 → `Idle` / 취소 → `Idle` |
| `Joining` | 발견한 기존 Lobby에 참가 중. (Client 관점) | `Searching` 중 Lobby 발견 | 참가 성공 → `Waiting` (Client 측 대기) / 실패 → `Idle` |
| `Ready` | 매칭 완료. 게임 시작 가능 상태. | Host: `Waiting` 정원 충족 / Client: MATCH_STARTED 감지 | `StartGame()` 호출 → `InGame` / 외부 취소 → `Idle` |
| `InGame` | ServerTravel 또는 ClientTravel 실행됨. 인게임 진행 중. | `StartGame()` 성공 | (Travel 후) Subsystem Deinitialize / 명시적 `ResetMatchState()` → `Idle` |

> **`Creating` 상태 결정 (v1.1 확정):** 본 Plan에서 `Creating` enum 값을 **명시적 상태로 살린다**. 이유:
> - Lobby 생성 호출(`LobbyProvider->CreateLobby()`)과 그 콜백 도착 사이에는 명백히 별개 단계가 존재한다 (네트워크 왕복 시간 포함).
> - FSM Enter/Exit 책임 매트릭스(§ 4.3)가 깔끔해진다 — `Searching → Creating → Waiting → Ready` 의 선형 흐름.
> - 디버깅 시 "현재 생성 요청 중인지 / 생성 후 대기 중인지"가 외부에서 즉시 식별 가능.
> - Phase A 진입 전 결정함으로써 구현 churn을 사전에 차단한다.
>
> **Client 측 `Waiting` 의미:** 기존 코드에서는 Host와 Client가 모두 "Lobby에 들어가서 대기"하는 상태를 같은 이름으로 부르지만, 행동이 다르다 (Host는 정원 카운트 폴링, Client는 MATCH_STARTED 폴링). FSM 진입 시 `bIsHost`를 함께 참조하여 `HandleEnterMatchState_Waiting()` 내부에서 분기한다. 별도 상태로 분리하지 않는 이유는, 외부에서 보기에는 모두 "대기 중"이라는 동일한 의미이며 분리 시 외부 API와 BP 노출이 복잡해지기 때문이다.

### 4.2 Transition Map — 허용된 전이의 명시

다음 표는 본 FSM이 허용하는 모든 상태 전이를 정의한다. 표에 없는 전이는 모두 거부되며, 시도 시 `ensureMsgf`로 즉시 감지된다.

| From → To | 조건 / 트리거 |
|---|---|
| `Idle → Searching` | `FindQuickMatch()` (멀티플레이) |
| `Idle → Creating` | `FindQuickMatch()` (SinglePlay) |
| `Searching → Joining` | Lobby 발견 |
| `Searching → Creating` | 검색 타임아웃 (호스트로 전환) |
| `Searching → Idle` | 취소 / 검색 실패 (재시도 한도 초과) |
| `Joining → Waiting` | 참가 성공 (Client) |
| `Joining → Idle` | 참가 실패 / 취소 |
| `Creating → Waiting` | 생성 성공 (Host) |
| `Creating → Idle` | 생성 실패 / 취소 |
| `Waiting → Ready` | Host: 정원 충족 + MATCH_STARTED 업데이트 완료 / Client: MATCH_STARTED 감지 |
| `Waiting → Idle` | 타임아웃 / 취소 |
| `Ready → InGame` | `StartGame()` 호출 성공 |
| `Ready → Idle` | 외부 취소 (`CancelMatch` / `ResetMatchState`) |
| `InGame → Idle` | `ResetMatchState()` 명시적 호출 (재매칭 시) / Subsystem Deinitialize |

**위반 사례 예시 (거부됨):**
- `Idle → Ready` (검색·생성·대기 단계를 건너뜀)
- `InGame → Searching` (Travel 후 상태 점프)
- `Waiting → InGame` (Ready 단계를 건너뜀)

### 4.3 Enter/Exit 책임 매트릭스

각 상태별로 진입(Enter) / 진출(Exit) 시 수행해야 할 작업을 명시한다. 이는 `HandleEnterMatchState` / `HandleExitMatchState` 내부 switch 구현의 명세가 된다.

| 상태 | OnEnter — 진입 시 수행 | OnExit — 진출 시 수행 |
|---|---|---|
| `Idle` | 상태 변수 초기화 (`bIsHost`, `CachedConnectString` 등 휘발성 캐시 클리어 — Strategy에 위임) | (없음 — Idle은 정적 상태) |
| `Searching` | Strategy의 `BeginSearchPhase` 호출 — Lobby 검색 시작 + 재시도 Ticker 등록 + `WaitStartTime` 캡처 | Strategy의 `EndSearchPhase` 호출 — 검색 재시도 Ticker 해제 + `OnFindComplete` 람다 정리 |
| `Joining` | Strategy의 `BeginJoinPhase` 호출 — Lobby 참가 시도 | Strategy의 `EndJoinPhase` 호출 — `OnJoinComplete` 람다 정리 |
| `Creating` | Strategy의 `BeginCreatePhase` 호출 — Lobby 생성 요청 | Strategy의 `EndCreatePhase` 호출 — `OnCreateComplete` 람다 정리 |
| `Waiting` | Strategy의 `BeginWaitPhase` 호출 — Host/Client 분기하여 폴링 Ticker 등록 | Strategy의 `EndWaitPhase` 호출 — 폴링 Ticker 해제 + UpdateSession 핸들 정리 |
| `Ready` | (선택) UI 알림용 짧은 안정화 작업 — 기본은 No-op | (선택) 진출 직전 Lobby 안내 정리 |
| `InGame` | (No-op — Travel은 `StartGame` 자체에서 동기적으로 시작됨. 진입 후에는 Subsystem이 더 이상 적극적 행동 안 함) | Subsystem Deinitialize 시 안전 정리 |

> **설계 메모:** Enter 작업과 외부 API 호출의 관계는 다음과 같이 정리된다. 외부 API(`FindQuickMatch` 등)는 **상태 전이만 요청** 하고, **실제 행동(EOS 호출, Ticker 등록 등)은 `HandleEnter*` 안에서 수행** 된다. 이로써 "API → 상태 변경 → 행동"의 흐름이 단방향으로 고정되어 추적이 단순해진다.

---

## 5. 단일 전이 진입점 설계 (TransitionMatchState)

### 5.1 함수 책임

`UExOnlineSubsystem::TransitionMatchState(EExMatchState NewState)` 는 다음 작업을 **이 순서대로** 수행한다:

1. **재진입 감지** — 현재 전이 진행 중이면 새 요청을 펜딩 큐(또는 단일 슬롯)에 보관 후 다음 틱에서 처리.
2. **소멸 가드** — Subsystem이 Deinitialize 중이면 무시.
3. **동일 상태 가드** — `NewState == CurrentMatchState` 면 경고 로그 후 무시 (Silent 비허용).
4. **전이 유효성 검증** — Transition Map 조회. 허용되지 않은 전이면 `ensureMsgf`로 알리고 무시.
5. **OldState Exit 호출** — `HandleExitMatchState(OldState)`.
6. **상태 갱신** — `CurrentMatchState = NewState`.
7. **NewState Enter 호출** — `HandleEnterMatchState(NewState)`.
8. **외부 알림 브로드캐스트** — `OnMatchStateChanged.Broadcast(OldState, NewState)`.
9. **전이 일관 로그 출력** — `LogExNetwork`에 `[FSM] OldState → NewState (Reason)` 포맷.
10. **펜딩 전이 처리** — 5~9 동안 누적된 요청이 있으면 다음 틱에 디스패치.

### 5.2 외부 API와의 매핑

| 외부 API | 내부 동작 |
|---|---|
| `FindQuickMatch(Config)` | Config 보관 후 `TransitionMatchState(Searching)` 또는 SinglePlay 분기로 `Creating` |
| `CancelMatch()` | 상태별 분기 — `Idle`/`InGame` 이면 무시, 그 외에는 `TransitionMatchState(Idle)` |
| `ResetMatchState()` | 무조건 `TransitionMatchState(Idle)` (디버그/강제 복원 용도). 단, 가능한 사용 빈도를 낮추는 것이 목표 — 정상 흐름은 `CancelMatch`를 우선 사용 |
| `StartGame(Config)` | 사전 검증(Ready 상태, MapPath, World 등) 통과 후 `TransitionMatchState(InGame)` |

### 5.3 재진입(Re-entrancy) 처리 정책 — v1.1 개정

EOS SDK 콜백 스택 안에서 동기 상태 전이가 발생하는 시나리오를 방지하기 위해, 다음 정책을 채택한다.

**v1.0의 단일 슬롯 last-wins 정책은 안전 우선 이벤트의 의도를 유실시킬 위험이 있어 폐기한다.** (외부 AI 피드백 #4 반영) 대신 다음 **우선순위 카테고리** 체계를 도입한다:

#### 5.3.1 전이 사유(Reason) 분류

모든 `TransitionMatchState` 호출에는 사유(Reason)를 명시한다. 사유는 다음 enum으로 표현한다:

| 사유 (`ETransitionReason`) | 분류 | 설명 |
|---|---|---|
| `UserRequest` | Normal | 외부 API 호출에 의한 정상 전이 (`FindQuickMatch` 등) |
| `AsyncCallback` | Normal | EOS Lobby Provider 콜백 결과로 인한 자연 전이 |
| `Timeout` | Normal | 대기 시간 만료로 인한 자동 전이 |
| `Cancel` | **Safety-First** | 사용자/시스템의 명시적 매칭 취소 |
| `Reset` | **Safety-First** | `ResetMatchState()` 호출 — 강제 복원 |
| `Deinitialize` | **Safety-First** | Subsystem 종료로 인한 정리 전이 |

#### 5.3.2 우선순위 규칙

- **Safety-First 사유는 최우선이다.** Normal 사유의 펜딩 슬롯에 무엇이 있든 즉시 덮어쓴다.
- **Safety-First가 펜딩 중인 동안 도착하는 Normal 전이 요청은 거부된다** — Warning 로그 후 무시. 사용자가 취소를 의도한 흐름에 후속 비동기 콜백이 끼어들지 못하게 한다.
- **Safety-First가 다른 Safety-First를 만난 경우는 마지막이 이긴다** (`Reset`이 `Cancel` 펜딩을 덮어쓰는 식). Safety-First끼리는 모두 같은 의도(매칭 중단)이므로 last-wins가 안전하다.
- **Normal vs Normal은 기존 last-wins 유지** — 일반 흐름에서 마지막 의도를 진실로 간주.

#### 5.3.3 펜딩 처리 메커니즘

- **단일 슬롯 + 우선순위 비교** — 슬롯에 새 요청이 들어올 때 우선순위 비교를 거쳐 채택/거부 결정.
- **펜딩 처리 시점** — 다음 게임 틱 (FTSTicker 1회성 등록).
- **펜딩 로그** — 펜딩 발생 시 사유와 함께 Warning 로그. Safety-First 펜딩은 Log 레벨로 별도 강조.
- **펜딩 무효화 조건** — Subsystem Deinitialize 시점 / `Idle` 도달 시 펜딩 슬롯 클리어.

#### 5.3.4 시나리오 검증 (피드백 #4 회귀 방지)

다음 시나리오는 본 정책으로 모두 안전하게 처리됨이 보장된다:

| 시나리오 | 결과 |
|---|---|
| Cancel 호출 → 직후 EOS `OnFindComplete` 도착 → `Joining` 전이 시도 | Cancel(Safety-First)이 펜딩 → Joining(Normal) 거부 → 사용자 의도 보존 |
| Reset 호출 → 직후 `OnCreateComplete` 도착 → `Waiting` 전이 시도 | Reset 우선 → Waiting 거부 |
| FindQuickMatch 호출 → 직후 같은 함수 재호출 | 두 번째 호출은 첫 번째와 동일 의도이므로 Normal last-wins로 무해 |
| Cancel → Reset 연속 호출 | Reset이 Cancel을 덮어씀 (둘 다 Safety-First, 결과적으로 `Idle` 진입) |

---

## 6. Strategy 책임 재정의

### 6.1 `IExNetServerStrategy` 베이스 인터페이스 보강 (v1.1 — 피드백 #1 반영)

기존의 `FindAndJoinOrCreate` 단일 진입점은 `FExListenServerStrategy`에만 존재했고, `UExOnlineSubsystem`은 이를 호출하기 위해 `static_cast<FExListenServerStrategy*>` 패턴을 5곳에 분산시켰다. 이는 Strategy 패턴의 의의를 훼손하며, Dedicated 전략 도입 시점에 런타임 캐스팅 실패 위험을 남긴다.

**v1.1에서는 Phase 메서드 군 전체를 베이스 인터페이스 `IExNetServerStrategy`의 가상 함수로 승격한다.** 이로써 `ExOnlineSubsystem.cpp`의 매칭 흐름 본체에서는 베이스 인터페이스 포인터만으로 모든 작업을 수행할 수 있게 되며, 캐스팅은 LobbyProvider 주입 등 1~2곳의 초기화 경로로 제한된다.

#### 6.1.1 베이스 인터페이스에 신설되는 가상 함수 (모두 순수 가상 또는 빈 기본 구현)

| 가상 함수 (의도 단위) | 책임 | 호출자 |
|---|---|---|
| `BeginSearchPhase(Config, ExpectedState, OnFoundCallback)` | Lobby 검색 시작 + 재시도 Ticker 등록 | `HandleEnterMatchState_Searching` |
| `EndSearchPhase()` | 검색 Ticker 해제 + 람다 정리 | `HandleExitMatchState_Searching` |
| `BeginJoinPhase(LobbyIndex, ExpectedState, OnJoinedCallback)` | 특정 Lobby 참가 시도 | `HandleEnterMatchState_Joining` |
| `EndJoinPhase()` | 참가 람다 정리 | `HandleExitMatchState_Joining` |
| `BeginCreatePhase(Config, ExpectedState, OnCreatedCallback)` | Lobby 생성 요청 | `HandleEnterMatchState_Creating` |
| `EndCreatePhase()` | 생성 람다 정리 | `HandleExitMatchState_Creating` |
| `BeginWaitPhase(Config, bIsHostFlag, ExpectedState, OnReadyCallback)` | Host/Client 분기하여 폴링 Ticker 등록 | `HandleEnterMatchState_Waiting` |
| `EndWaitPhase()` | 폴링 Ticker 해제 + UpdateSession 핸들 정리 | `HandleExitMatchState_Waiting` |
| `IsHost() const` | 현재 인스턴스가 호스트로 동작 중인지 | Subsystem |
| `GetConnectString() const` | Client가 호스트 세션에 연결할 ConnectString | `StartGame` 내부 (Client 측) |
| `CancelMatch()` | 진행 중인 매칭 즉시 취소 (모든 휘발성 상태 정리) | `HandleEnterMatchState_Idle` 또는 안전 정리 경로 |
| `ResetTransientState()` | 휘발성 캐시 일괄 클리어 (§ 6.4 참조) | `HandleEnterMatchState_Idle` |
| `StartGameSession(MapPath, World)` | (기존 유지) ServerTravel 실행 | `StartGame` 내부 (Host 측) |
| `CreateMatch(Config) / JoinMatch(SessionId) / DestroyMatch()` | (기존 유지) | 호환성 유지용. Phase 메서드 도입 후 사용 빈도 감소 가능 |

#### 6.1.2 `FExDedicatedServerStrategy`의 처리

본 Plan은 Dedicated 전략의 **실제 로직 구현은 범위 외**다 (§ 1.2). 다만 베이스 인터페이스가 위처럼 확장되므로, Dedicated 구현체는 각 가상 함수에 대해 다음 중 하나로 둔다:

- **빈 구현 + 경고 로그** — `UE_LOG(LogExNetwork, Warning, TEXT("[Dedicated] Not yet implemented: %s"), TEXT("BeginSearchPhase"));`
- **`ensureMsgf` 사용** — 호출 즉시 개발자가 인지할 수 있도록 (가이드라인 1.7 준수)

이 처리는 Phase B 구현 시점에 일괄 추가되며, 향후 Dedicated FSM Plan에서 실제 동작으로 채워진다.

#### 6.1.3 `ExOnlineSubsystem.cpp`의 캐스팅 청산

본 Plan 구현 완료 시 다음 변화가 보장된다:

| 위치 | v1.0 (현재) | v1.1 (목표) |
|---|---|---|
| `FindQuickMatch` | `static_cast<FExListenServerStrategy*>` 후 `FindAndJoinOrCreate` 호출 | `ServerStrategy->BeginSearchPhase()` 직접 호출 |
| `CancelMatch` | `static_cast<...>` 후 `CancelMatch` 호출 | `ServerStrategy->CancelMatch()` 직접 호출 |
| `ResetMatchState` | `static_cast<...>` 후 `CancelMatch` 호출 | `ServerStrategy->ResetTransientState()` 직접 호출 |
| `StartGame` | `static_cast<...>` 후 `IsHost / GetConnectString` 조회 | 베이스 인터페이스로 직접 조회 |
| Initialize 경로 (LobbyProvider 주입) | Listen 전용 동작이므로 캐스팅 유지 | **유일하게 허용되는 캐스팅 위치** — 명시적으로 그 의도 주석 처리 |

### 6.2 콜백 시그니처 표준화 — ExpectedState 토큰 (v1.1 — 피드백 #5 반영)

Strategy의 모든 Phase 콜백은 다음 두 가지 결과를 통보한다. 어떤 다음 상태로 가야 할지는 Subsystem이 결정한다 (책임 분리 원칙).

#### 6.2.1 콜백 기본 시그니처

- **성공/실패 + 에러 메시지** — `TFunction<void(bool bSuccess, const FString& ErrorMessage)>`
- **추가 정보 (Phase별)** — 검색의 경우 `ResultCount`, 참가의 경우 `ConnectString` 등 의미가 다른 보조 데이터는 Phase별 콜백 형태 또는 Strategy의 getter로 제공

#### 6.2.2 ExpectedState 토큰 강제 규칙 (v1.1 신규)

**문제:** EOS 비동기 콜백은 매우 늦게 (수 초~수십 초 후) 도착할 수 있고, 그 사이 사용자가 매칭을 취소했거나 다른 흐름이 진행되었을 수 있다. 콜백 진입 시점의 현재 상태가 콜백을 발사한 시점의 상태와 다르면, 콜백 결과를 적용하면 안 된다.

**규칙:**

1. **Strategy의 모든 Begin 호출은 호출 시점의 `EExMatchState ExpectedState` 토큰을 인자로 받는다** (§ 6.1.1 표 참조). Subsystem이 `BeginSearchPhase(Config, EExMatchState::Searching, ...)` 형태로 호출 시 자신의 상태를 토큰으로 전달.
2. **Strategy는 콜백 람다에 이 토큰을 캡처한다.** 콜백 발사 시점에 함께 전달하거나, Strategy가 자신의 마지막 ExpectedState를 멤버로 보관.
3. **Subsystem의 콜백 핸들러 진입부에서 토큰을 비교한다.** 토큰이 `GetMatchState()`와 일치하지 않으면 **무조건 무시 + Warning 로그**.
4. **헬퍼 함수로 캡슐화** — `bool UExOnlineSubsystem::ShouldHonorCallback(EExMatchState ExpectedState, const TCHAR* CallbackName) const` 형태로 제공하여 모든 콜백 진입부에서 1줄로 호출 가능.

#### 6.2.3 적용 의무 범위

ExpectedState 토큰 비교는 다음 모든 콜백 진입부에서 **강제** 한다:

- `BeginSearchPhase` 결과 콜백 (`OnFindComplete`)
- `BeginJoinPhase` 결과 콜백 (`OnJoinComplete`)
- `BeginCreatePhase` 결과 콜백 (`OnCreateComplete`)
- `BeginWaitPhase` 결과 콜백 (Host/Client 모두)
- EOS SDK의 `OnUpdateSessionComplete` (MATCH_STARTED 업데이트 콜백)
- 그 외 Strategy → Subsystem 방향의 모든 비동기 콜백

이 규칙은 § 11.2 리스크 매트릭스 "비동기 콜백 지연 도착" 항목의 완화책으로 격상된다.

### 6.3 Strategy 내부 휘발성 캐시 관리

`bIsHost`, `CachedConnectString`, `WaitStartTime`, `CurrentWaitConfig`, `UpdateSessionHandle`, `bIsDestroyed`, `FindRetryCount` 등은 Strategy의 내부 휘발성 상태로 유지된다. 단, **이 값들의 초기화 시점**을 다음과 같이 명확히 한다:

- **각 Phase의 Begin 시점**에 해당 Phase가 사용할 값만 초기화한다 (예: `BeginSearchPhase`에서 `WaitStartTime` 캡처).
- **`Idle` 진입의 Strategy 콜백** (`ResetTransientState`)을 새로 추가하여, 매칭 완전 종료 시 모든 휘발성 캐시를 일괄 클리어한다. 이는 `HandleEnterMatchState_Idle`에서 호출된다.

### 6.4 ResetTransientState 책임

신규 메서드 `FExListenServerStrategy::ResetTransientState()`는 다음을 클리어한다:
- `bIsHost = false`
- `CachedConnectString = TEXT("")`
- `WaitStartTime = 0.0`
- `FindRetryCount = 0`
- `CurrentWaitConfig = FExMatchConfig()` (기본값)
- 모든 LobbyProvider 델리게이트 `Clear()`
- 모든 Ticker 핸들 해제 (이미 `EndXxxPhase`에서 했어야 하지만 이중 안전망)
- **(v1.2 신규)** 로비 세션 파괴 — `LobbyProvider->IsInLobby()`가 `true`일 경우 반드시 `LobbyProvider->DestroyLobby()`를 호출하여 잔여 세션이 확실히 정리되도록 보장한다.

이 메서드가 매칭 종료 후 다음 매칭 진입 시 **이전 매칭의 잔여 상태가 새 매칭에 누수되는 문제**를 원천 차단한다.

---

## 7. 외부 알림 인터페이스 (Public Notification API)

### 7.1 신규 델리게이트 — `OnMatchStateChanged`

`UExOnlineSubsystem`에 다음 BP 노출 델리게이트를 추가한다.

| 항목 | 정의 |
|---|---|
| 시그니처 | `(EExMatchState OldState, EExMatchState NewState)` 2-Params |
| 매크로 | `DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams` (BP 호출 호환) |
| 노출 | `UPROPERTY(BlueprintAssignable, Category = "ExNetwork|Match")` |
| 호출 시점 | `TransitionMatchState` 8단계에서 매 전이마다 1회 |
| 호출 보장 | Enter 호출 **이후**에 브로드캐스트 — UI가 외부 알림을 받았을 때는 이미 진입 작업이 시작된 시점 |

> **UI 통합 전제 조건 (v1.1 명시):** 본 델리게이트가 도입되면 UI ViewModel(`UExLobbyMatchViewModel` 등)은 자체 상태 플래그(`bIsMatching`, `bPendingStartMultiPlay` 등) 없이 이 채널만으로 모든 매칭 상태를 추적할 수 있게 된다. 이는 후속 작업 `MatchUI_FSM_Integration_Plan` (가칭)의 **전제 조건이자 기반 인프라**다. 본 Plan 단계에서는 채널 제공까지만 진행하며, UI 측 구독 작업은 의도적으로 분리된다 (§ 1.2 참조).

### 7.2 기존 델리게이트와의 관계 — v1.1 계약 명시 강화 (피드백 #2 반영)

| 기존 델리게이트 | 변경 여부 | 비고 |
|---|---|---|
| `OnLoginComplete` | 변경 없음 | 인증 흐름은 본 Plan 범위 외 |
| `OnMatchFound` | **의미 정제** — `Ready` 상태 진입 시점에 1회 브로드캐스트 (성공 시) / `Idle` 폴백 시 1회 브로드캐스트 (실패 시). 즉 결과 알림 채널 역할로 명확화 | 시그니처 유지 |
| `OnGameStarted` | **의미 정제 + 계약 명시 (v1.1)** | 시그니처 유지 |

#### 7.2.1 `OnGameStarted` 계약 명시 (v1.1 신규)

현행 구현은 `OnGameStarted.Broadcast(true)` 호출 **이후에** `ServerTravel` 또는 `ClientTravel`을 실행한다. 즉 이름은 "Started"지만 실제로는 **"Travel 시작 요청이 수락된 시점"** 이지 "맵 로드 완료"가 아니다. 이 차이가 향후 회귀의 원인이 될 수 있으므로, **계약(Contract)을 명시적으로 못 박는다**:

> **`OnGameStarted` 계약:**
> - 본 델리게이트는 **"Travel 시작 요청이 수락되었다"** 는 신호이지, 실제 맵 로드 완료를 보장하지 않는다.
> - `bSuccess = true` 시: Host는 `ServerTravel` 호출 직전, Client는 `ClientTravel` 호출 직전 시점에 발생.
> - **맵 로드 완료 시점은 별도 채널로 인지해야 한다** — 게임 맵 측 `GameMode::BeginPlay` 또는 `GameState::OnRep_*` 등을 통해 인지. 본 델리게이트로 그것을 대체하지 말 것.
> - `bSuccess = false` 시: 사전 검증 실패(로그인 미완료 / 상태 불일치 / MapPath 누락 / World 없음 / Strategy 없음)로 Travel 자체가 시도되지 않은 경우.

#### 7.2.2 호출 순서 보장

`StartGame` 내부 호출 순서는 다음으로 고정한다:

1. 사전 검증 (실패 시 `OnGameStarted.Broadcast(false, ErrorMessage)` 후 즉시 반환)
2. `TransitionMatchState(InGame, ETransitionReason::UserRequest)`
3. `OnGameStarted.Broadcast(true, TEXT(""))` — Travel 시작 요청 수락 알림
4. Host: `ServerTravel(...)` / Client: `ClientTravel(...)`

순서 3과 4가 **반드시 이 순서**임을 코드 주석으로도 명시한다. Travel 직후 콜백 스택 안에서 동기 처리가 발생할 가능성을 고려하여, 사용자가 알림을 받는 시점은 Travel 직전이어야 한다.

#### 7.2.3 추후 이름 변경 검토 (별도 작업)

이름 자체가 오해를 유발하므로, 향후 BP 호환성 영향 분석 후 다음 후보로 이름 변경을 검토한다 (본 Plan 범위 외):
- `OnGameStartRequested`
- `OnTravelInitiated`

이는 별도 마이그레이션 작업으로 분리하며, 본 Plan에서는 **계약 명시까지만 진행**한다.

### 7.3 이중 알림 정책 요약

`OnMatchStateChanged`는 모든 상태 변화에 대해 발생하고, `OnMatchFound`/`OnGameStarted`는 특정 결과 시점에만 발생한다. 두 채널은 의도적으로 공존한다 — 전자는 디버그/UI 상태 추적용, 후자는 외부 비즈니스 로직의 결과 처리용이다. UI는 둘 중 어느 것을 구독해도 되지만, 일반적으로 상태 머신 추적은 `OnMatchStateChanged`, 결과 행동은 `OnMatchFound`/`OnGameStarted`를 권장한다.

### 7.4 BP 호출용 헬퍼 (선택)

가이드라인 1.7 "Silent Failure 방지" 차원에서, BP에서 상태 점검을 단순화하는 헬퍼를 함께 제공한다:
- `IsMatchInProgress()` — `Idle` / `InGame` 외 상태이면 true (매치 진행 중 판정).
- `IsMatchReadyToStart()` — `Ready` 상태이면 true.
- `IsMatchActive()` — `InGame` 상태이면 true.

이들은 BP 노출 (`UFUNCTION(BlueprintPure)`)로 제공되어, BP가 enum 직접 비교에 의존하지 않게 한다.

---

## 8. 데이터 구조 변경 (Data Structure Changes)

### 8.1 `EExMatchState` enum 자체

- 값 정의는 유지하되, 각 값에 대해 의미를 명확히 설명하는 헤더 주석을 강화한다.
- **(v1.1 확정)** `Creating` enum 값은 **명시적 상태로 살린다** (§ 4.1 참조). 사유:
  - `LobbyProvider->CreateLobby()` 호출과 콜백 도착 사이의 네트워크 왕복 구간이 별개 단계로 식별 가능해야 한다.
  - FSM Enter/Exit 책임 매트릭스가 선형 흐름(`Searching → Creating → Waiting → Ready`)으로 단순해진다.
  - Phase A 진입 전 결정함으로써 구현 도중 enum 변경으로 인한 churn을 사전 차단한다.

### 8.2 신규 내부 데이터 멤버 (`UExOnlineSubsystem`)

다음 내부 멤버가 추가된다. 모두 비-Replicated이며, GameInstance Subsystem 로컬 상태다.

| 멤버 | 타입 | 용도 |
|---|---|---|
| 펜딩 전이 슬롯 | `TOptional<TPair<EExMatchState, ETransitionReason>>` | 재진입 시 다음 틱 처리용. **v1.1: Reason 포함** (Safety-First 우선순위 비교용) |
| 펜딩 처리 Ticker 핸들 | `FTSTicker::FDelegateHandle` | Subsystem 소멸 시 안전 해제 |
| 현재 전이 진행 플래그 | `bool` | 재진입 감지용 |
| 마지막 전이 사유 (디버그용) | `FString` | 로그 출력용. 옵션 |
| 현재 매치 Config 캐시 | `FExMatchConfig` | `FindQuickMatch` 시 보관, 상태 전이 시 Strategy에 전달 |

### 8.3 Transition Map 정적 데이터

전이 맵은 `Initialize()` 시점에 1회 빌드되며, 변경 불가능한 정적 데이터다.

| 옵션 | 권장 |
|---|---|
| `TMap<EExMatchState, TArray<EExMatchState>>` 멤버 변수 | ✅ 권장 — 가독성, 디버거 시각화 용이 |
| `static constexpr` 2D 배열 | 가능하나 enum 인덱싱 변환 보일러플레이트 발생 |
| INI/DataAsset에서 로드 | 과한 설계 — 전이 규칙은 코드 일관성과 일치해야 함 |

### 8.4 가이드라인 1.7 준수 — 필수 포인터 검증

`ServerStrategy`, `AuthProvider`, `World` 등 FSM 진입/전이 중 사용되는 모든 핵심 포인터에 대해 `if + ensureMsgf` 패턴을 적용한다. Silent Failure로 상태 전이가 사일런트하게 무시되는 것을 막는다.

---

## 9. 비기능 요구사항 (Non-Functional Requirements)

### 9.1 성능

- FSM 디스패치는 매 전이당 1회만 발생하며, 정상 매칭 흐름 전체에서 10회 미만의 전이만 일어난다. `switch` 디스패치 오버헤드는 사실상 무시 가능.
- Tick 기반 호출 금지 — FSM은 이벤트 주도(event-driven)다. 폴링은 Strategy 내부의 기존 Ticker가 담당하며, 결과 발생 시점에만 Subsystem으로 전이를 요청한다.

### 9.2 안전성

- Subsystem `Deinitialize` 시 다음을 보장한다:
  - 펜딩 Ticker 해제.
  - 현재 진행 중인 Phase의 `EndXxxPhase` 강제 호출 (현재 상태에 대응되는 Exit 1회 실행).
  - Strategy 소멸 전에 모든 LobbyProvider 콜백 정리.
- 펜딩 전이는 Subsystem 소멸 시 무효화된다.

### 9.3 디버깅성

- 단일 로그 포맷: `[ExMatchFSM] [Searching → Joining] reason=LobbyFound resultCount=2`.
- 디버그 패널 친화 — `GetMatchState()`와 `OnMatchStateChanged`만으로 외부에서 상태 흐름을 완전 재구성 가능.
- 디버그 빌드에서 강제 전이를 허용하는 별도 함수를 가이드라인 1.11에 따라 본 로직과 분리된 함수로 제공.

### 9.4 테스트 용이성

- FSM 디스패치 자체는 외부 의존성이 없어, 향후 단위 테스트 도입 시 `TransitionMatchState`의 전이 규칙 검증만으로도 핵심 로직 회귀 방지가 가능.
- Phase 콜백을 표준 시그니처로 통일하므로, Strategy 모킹 시 인터페이스가 단순.

---

## 10. 구현 단계 (Implementation Phases)

### Phase A — Transition 골격만 도입 (외부 동작 불변 검증)

목표: 외부에서 보기에 동작이 완전히 동일한 상태에서 내부 전이 구조만 교체한다.

1. `UExOnlineSubsystem`에 다음 추가:
   - `TransitionMatchState` 단일 진입점
   - `HandleEnterMatchState` / `HandleExitMatchState` 빈 switch 골격
   - Transition Map 빌드 (`Initialize`)
   - `OnMatchStateChanged` 델리게이트
   - 재진입 가드 및 펜딩 슬롯
2. 기존 직접 대입 `CurrentMatchState = X;` 5곳을 모두 `TransitionMatchState(X)` 호출로 치환.
3. 이 단계까지는 Strategy 인터페이스 변경 없음. `Enter`/`Exit` 핸들러는 비어 있고, 모든 작업은 기존 그대로 외부 호출자에서 수행됨.
4. 검증: 기존 매칭 시나리오 (싱글/멀티 / Host/Client / 정상/타임아웃/취소) 전 케이스가 동일하게 동작하는지 확인.

### Phase B — Strategy 책임 이전

목표: `FindAndJoinOrCreate` 단일 진입점을 Phase 단위 메서드로 분리하고, 행위 코드를 `HandleEnter*`로 이전.

1. Strategy에 `BeginSearchPhase / EndSearchPhase / BeginCreatePhase / ...` 메서드 군 추가.
2. 기존 `FindAndJoinOrCreate` 내부 로직을 각 Begin 메서드로 분리. 콜백 시그니처 표준화.
3. `HandleEnterMatchState_Searching` 등에서 Strategy의 Begin 메서드를 호출.
4. `HandleExitMatchState_Searching` 등에서 Strategy의 End 메서드를 호출.
5. `ResetTransientState` 추가, `HandleEnterMatchState_Idle`에서 호출.
6. 검증: 동일 시나리오 전 케이스 재검증. 특히 취소/타임아웃 직후 재매칭 시 잔여 상태 누수 없음을 확인.

### Phase C — 외부 알림 및 BP 헬퍼 완성

1. `OnMatchStateChanged` 외부 노출 활성화 및 브로드캐스트 위치 확정.
2. `IsMatchInProgress / IsMatchReadyToStart / IsMatchActive` BP 헬퍼 추가.
3. **(v1.1 변경)** `Creating` enum은 § 4.1 / § 8.1에서 이미 살림 결정됨 — Phase C에서는 결정 작업 없이 § 4.3 책임 매트릭스대로 진입 경로(Lobby 생성 요청 시점)와 진출 경로(생성 콜백 완료 시점)를 단순히 연결하기만 한다.
4. 디버그 강제 전이 함수 추가 (`!UE_BUILD_SHIPPING` 가드).
5. 검증: 매칭 위젯이 새 델리게이트만으로 모든 상태 흐름을 추적 가능한지 확인.

### Phase D — 문서 동기화 (코드 외 작업)

1. `Md/Architecture/ExCore/ExFrameWork_Multiplayer_Flow_Architecture.md` v2.1로 갱신:
   - § 2.x 신규 항목: "Flow.Lobby 내부 매치메이킹 sub-state (EExMatchState 계층)"
   - 전이 다이어그램 추가.
2. `Md/Architecture/ExCore/` 하위에 본 FSM 설계 결과를 요약한 아키텍처 문서 신설 또는 본 Plan을 Architecture로 승격 이관.
3. 멀티플레이 버그가 향후 발생할 경우 본 FSM 시점을 기준으로 진단하도록 `Md/Bug/MultiPlay/` 트러블슈팅 가이드 추가.

---

## 11. 사이드 이펙트 및 리스크 (Risks & Side Effects)

### 11.1 사이드 이펙트

| 영역 | 영향 |
|---|---|
| 외부 BP/C++ 호출자 | 외부 API 시그니처 유지 → **영향 없음** |
| 매칭 위젯 / HUD | 신규 델리게이트 구독 가능 (선택 사항). 기존 `OnMatchFound`/`OnGameStarted` 구독은 그대로 동작 |
| 로그 출력 | 상태 전이 로그가 일관된 포맷으로 통일됨 → 디버깅 이득 |
| Strategy 내부 코드 구조 | 함수가 잘게 쪼개짐 → 단기적으로 코드 라인 수 증가, 장기적으로 책임 단위 명확화 이득 |
| Dedicated Server 전략 | 본 Plan 범위 외이지만, 동일 패턴을 강제할 기반 마련됨 (후속 Plan에서 자연스럽게 적용 가능) |

### 11.2 리스크 및 완화책 — v1.1 갱신

| 리스크 | 완화책 |
|---|---|
| 비동기 콜백 스택 안에서 동기 전이 발생 시 크래시 | 재진입 가드 + 펜딩 슬롯으로 다음 틱 지연 처리 |
| 기존 멀티플레이 시나리오 회귀 | Phase A에서 외부 동작 불변을 먼저 보장한 후 Phase B에서 단계적 이전. 각 Phase 종료 시 동일 시나리오 전 케이스 재검증 |
| Subsystem Deinitialize와 펜딩 전이의 경합 | Deinitialize 시 펜딩 슬롯 및 Ticker 핸들 명시적 해제. `bIsDestroyed` 플래그 검사 통합. **v1.1**: Safety-First 사유 `Deinitialize`로 명시적 전이 |
| `Creating` 상태의 의미 모호성 | **v1.1 확정**: 명시적 상태로 살림 (§ 4.1 / § 8.1) — Phase A 전 사전 결정 완료 |
| Strategy의 휘발성 상태 누수 (재매칭 시) | `ResetTransientState` 도입 + `Idle` 진입 시 무조건 호출 |
| EOS Lobby의 비동기 결과 도착 시 이미 상태가 전이된 경우 (예: 취소 직후 `OnFindComplete` 도착) | **v1.1 강화**: 모든 비동기 콜백에 `ExpectedState` 토큰 강제 (§ 6.2.2). 콜백 진입부에서 `ShouldHonorCallback(Expected)` 헬퍼 1줄 호출로 일관 검증 |
| **(v1.1 신규)** Cancel/Reset 의도가 후속 일반 전이에 덮어쓰여 유실 | Safety-First 우선순위 카테고리 도입 (§ 5.3) — `Cancel/Reset/Deinitialize` 사유는 펜딩 슬롯의 Normal 사유를 무조건 덮어쓰며, 그 후 도착하는 Normal 요청은 거부 |
| **(v1.1 신규)** Dedicated 환경에서 Listen 전용 캐스팅 실패 위험 | `IExNetServerStrategy` 베이스 인터페이스 보강(§ 6.1) — `ExOnlineSubsystem.cpp`의 매칭 흐름 본체에서 캐스팅 패턴 제거. Dedicated 구현은 빈 구현 또는 `ensureMsgf`로 미구현 명시 |
| **(v1.1 신규)** `OnGameStarted`가 "맵 로드 완료"로 오해되는 회귀 | § 7.2.1 계약 명시 — "Travel 시작 요청 수락"으로 못 박음. 호출 순서(§ 7.2.2)를 코드 주석에 명시. 맵 로드 완료는 `GameMode::BeginPlay` 등 별도 채널 사용 |
| **(v1.1 신규)** UI ViewModel 자체 상태 플래그와 FSM 상태의 진실 소스 분리 | 본 Plan에서는 `OnMatchStateChanged` 채널만 제공 (§ 7.1). 후속 `MatchUI_FSM_Integration_Plan`에서 ViewModel 자체 플래그를 제거하고 단일 진실 소스로 통합 |
| **(v1.2 신규)** `InGame` 상태 중 Network Travel Failure (이동 실패) 발생 | GEngine의 `OnNetworkFailure` 또는 `OnTravelFailure` 델리게이트를 Subsystem 초기화 시 구독하여, 에러 발생 시 `InGame → Idle`로 자동 복구(`ResetMatchState`)하도록 예외 전이 트리거 추가 |
| **(v1.2 신규)** `IExNetServerStrategy` 명명에 대한 아키텍처적 모호성 | 서버/클라이언트 로직이 혼재되어 있으나 본 Plan에서 명명 리팩토링은 유보. 추후 클라이언트/서버 로직 비대화 시 `IExNetMatchStrategy` 등 명칭 변경 또는 분리 검토를 기술 부채로 기록 |

### 11.3 명시적으로 다루지 않는 사항 — v1.1 명문화

- 상위 `UExGameFlowSubsystem`과의 연동 (별도 Plan: `ExGameFlow_Transition_Plan.md` 영역)
- Match.* GameplayTag 계층 (인게임 매치 상태)
- Dedicated Server 흐름의 **실제 매칭 로직 구현** (후속 Plan). 단, **베이스 인터페이스 보강은 본 Plan 범위에 포함됨**
- **(v1.1 명문화)** 매치메이킹 위젯(`UExLobbyMatchViewModel`)의 신규 델리게이트 구독 및 자체 상태 플래그 제거 — 본 Plan 완료 후 후속 작업 `MatchUI_FSM_Integration_Plan` (가칭)에서 단일 진실 소스로 통합. 본 Plan 단계에서는 채널 제공만 진행하며, UI 로직 변경은 의도적으로 분리

---

## 12. 외부 AI 피드백 검토 절차 (Review Protocol)

본 Plan은 가이드라인 3.1 "주요 변경 사항 사전 보고" 및 ExFrameWork의 표준 외부 AI 피드백 검토 절차를 따른다. 다음 순서로 진행한다:

1. 본 Plan을 외부 AI에 검토 의뢰.
2. 피드백 항목별로 다음 셋 중 하나로 처리:
   - **통합 (Accept)** — 본 Plan에 반영, v1.1로 개정.
   - **명시적 거부 (Reject with Rationale)** — 거부 사유를 Plan 부록에 기록.
   - **유보 (Defer)** — 후속 Plan으로 이연 결정 사유와 함께 기록.
3. 사용자 최종 승인 후 구현 진입.

---

## 13. 요약 — 본 Plan의 한 줄 가치

> 누적된 if 가드를 **단일 전이 진입점 + Enter/Exit + 전이 유효성 검증** 으로 치환하여, 매치메이킹 흐름의 **상태 추적 가능성·재진입 안전성·확장성**을 구조적으로 확보한다. 외부 API는 보존하므로 호출자에 미치는 영향은 없으며, 핵심 작업은 4개 Phase(A → B → C → D)로 점진 이전이 가능하다.


---

## 14. Revision History (v1.1 신규)

### v1.1 — 2026-05-15 — 외부 AI 피드백 검토 반영본

본 개정은 v1.0에 대한 외부 AI 검토 피드백 6건을 항목별로 검토·수용한 결과를 반영한다. 모든 인용 코드 경로는 작성자가 직접 검증 후 반영 여부를 판단하였다.

#### 수용 결정 매트릭스

| # | 피드백 요지 | 우선순위 | 검토 결론 | 반영 위치 |
|---|---|---|---|---|
| 1 | Dedicated 캐스팅 안전성 — `static_cast<FExListenServerStrategy*>` 분산 | High | **부분 수용** — Dedicated 실제 구현은 범위 외 유지, 단 베이스 인터페이스 보강을 본 Plan에 포함하여 캐스팅 패턴 자체를 청산 | § 1.1 / § 1.2 / § 6.1 / § 11.2 |
| 2 | `OnGameStarted` 의미 모호 — "Travel 완료"로 오해 가능 | High | **전적 수용** — "Travel 시작 요청 수락" 계약 명시 + 호출 순서 명문화. 이름 변경 자체는 별도 작업 | § 7.2.1 / § 7.2.2 / § 7.2.3 / § 11.2 |
| 3 | UI 상태 이중화 — ViewModel 자체 플래그와 FSM 상태 분리 | Medium | **유보 + 명시화** — 후속 `MatchUI_FSM_Integration_Plan` (가칭)으로 분리. 본 Plan은 `OnMatchStateChanged` 채널 제공까지만 진행하며 전제 조건임을 명문화 | § 1.2 / § 7.1 / § 11.2 / § 11.3 |
| 4 | 재진입 정책 last-wins로 Cancel 의도 유실 위험 | Medium | **전적 수용** — Safety-First 우선순위 카테고리(`Cancel/Reset/Deinitialize`) 도입. `ETransitionReason` enum 신설 | § 5.3 (전면 재작성) / § 8.2 / § 11.2 |
| 5 | 비동기 콜백 ExpectedState 토큰 강제 규칙 부재 | Medium | **전적 수용** — 모든 Begin 호출에 `ExpectedState` 인자 의무화, 콜백 진입부에 `ShouldHonorCallback` 헬퍼 1줄 호출 강제 | § 6.1.1 / § 6.2.2 / § 6.2.3 / § 11.2 |
| 6 | `Creating` 상태 결정 시점 — 구현 단계 유보로 churn 위험 | Low | **전적 수용** — Phase A 진입 전 사전 결정: `Creating` 살림 확정 | § 4.1 / § 8.1 / § 10 (Phase C 단계 갱신) / § 11.2 |

#### 거부/유보 항목

- **거부:** 없음
- **유보(후속 Plan으로 분리):**
  - 피드백 #1의 Dedicated 실제 구현 — 후속 Dedicated FSM Plan
  - 피드백 #3의 ViewModel 자체 플래그 제거 — 후속 `MatchUI_FSM_Integration_Plan` (가칭)
  - 피드백 #2의 `OnGameStarted` 이름 변경 — 별도 마이그레이션 작업

#### v1.0 → v1.1 주요 구조 변경

1. **`ETransitionReason` enum 신설** — Normal / Safety-First 두 카테고리로 전이 사유 분류 (§ 5.3.1).
2. **`TransitionMatchState` 시그니처 확장** — `(NewState, Reason)` 두 인자 (§ 1.1 / § 5.3).
3. **펜딩 슬롯 타입 확장** — `TOptional<TPair<EExMatchState, ETransitionReason>>` (§ 8.2).
4. **`IExNetServerStrategy` 베이스 인터페이스 보강** — Phase 메서드 군 + 공통 조회 함수 전체 승격 (§ 6.1.1).
5. **`ShouldHonorCallback` 헬퍼 신설** — 모든 비동기 콜백 진입부 강제 (§ 6.2.2).
6. **`Creating` 상태 살림 확정** (§ 4.1 / § 8.1).
7. **`OnGameStarted` 계약 문구 명시** (§ 7.2.1).
8. **후속 작업 명시화** — `MatchUI_FSM_Integration_Plan` 가칭 후속 작업으로 명문화 (헤더 / § 1.2 / § 11.3).

### v1.0 — 2026-05-15 — 초안 작성

- 매치메이킹 흐름의 암묵적 상태 머신 문제 진단
- 경량 FSM 패턴 채택 (단일 전이 진입점 + Enter/Exit + 전이 유효성 검증)
- 4개 Phase 점진 이전 계획 (A → B → C → D)
- 외부 API 시그니처 보존 원칙 확립
