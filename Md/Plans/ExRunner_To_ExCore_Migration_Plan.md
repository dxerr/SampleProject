# ExRunner → ExCore 이관 및 공용 시스템화 계획

> **버전:** v2.0
> **최종 수정:** 2026-06-22 (현재 코드 기준 전면 재검증: 결합도 측정, 신규 후보 추가, 디버그 도구 분리, 등급/우선순위 정리)
> **프로젝트:** ExFrameWork

## 개요
새로운 GameFeature 기반 게임 모드를 제작하기 위해, `ExRunnerPlay` 모듈에 종속된 시스템 중 **범용적으로 재사용 가능한 기능**을 찾아 `ExCore`로 이관·추상화하는 계획입니다. 이를 통해 어떤 장르의 GameFeature를 추가하더라도 `ExCore`의 뼈대를 그대로 재사용할 수 있게 합니다.

> ⚠️ **선행 조건 (Phase 0):** 본 마이그레이션 착수 전, **UE 5.7.4 → 5.8 엔진 업그레이드**를 먼저 완료해야 합니다. 엔진 API 깨짐과 구조 리팩토링을 한 빌드에 뒤섞지 않기 위함입니다. 상세는 [`UE5.8_Engine_Upgrade_Plan.md`](UE5.8_Engine_Upgrade_Plan.md) 참조. 5.8 기준선(빌드/PIE/서버) 합격 후 아래 Phase 1을 착수합니다.

---

## 0. 판정 기준 및 결합도 측정

판정 기준은 `ExFrameWork_Guidelines.md` §4.1 — *"이 로직이 러너에만 필요한가 vs 다른 장르에서도 쓰이는가"* 입니다.
각 시스템의 `.cpp`가 참조하는 **Runner 전용 타입 결합도**를 측정하여 등급을 매겼습니다.

| 등급 | 의미 |
|---|---|
| 🟡 | **추상화 후 승격** — 개념은 범용, Runner 타입 결합을 베이스 클래스/이벤트로 분리 필요 |
| 🔴 | **Feature 유지** — 러너 게임플레이 본질 |
| 🐞 | **디버그 도구** — 프로덕션 승격 대상 아님 (필요 시 `ExCore/Debug`로 별도 일반화) |

> 본 분석에서 "결합도 0 → 즉시 승격(🟢)" 후보로 잠정 분류했던 `UExRunnerPositionSyncComponent`는, 실제 코드 확인 결과 **서버/클라 위치 오차를 DrawDebug로 시각화하는 디버그 컴포넌트**임이 밝혀져 후보에서 제외되었습니다 (§3 참조).

---

## 1. 승격 후보 (🟡 추상화 후 이관)

각 항목: **현재 클래스 → 목표(ExCore) / 결합 근거 / 작업 / 난이도**

### 1.1 Stat & Score 시스템
- **현재**: `UExRunnerStatComponent` (점수·코인·거리 등 상태값 보관)
- **목표**: `UExStatComponent` → `ExCore/Components`
- **결합 근거**: `AExRunnerGameState`×2 (그 외 결합 없음 — 낮음)
- **작업**:
  - GameState 참조를 `AExGameStateBase`로 일반화.
  - **[중요] ExCore에 이미 `ExPlayerStatsViewModel`(UI/ViewModels)이 존재**하므로 역할을 명확히 분담: **데이터 보관 = `ExStatComponent`, 표시 = `ExPlayerStatsViewModel`**.
  - **[확장성]** HP/MP 등 RPG 요소까지 담을 수 있는 범용 스탯 구조(키-값 또는 태그 기반)를 염두에 두고 설계.
- **난이도**: 낮음~중간

### 1.2 Buff & Status Effect 시스템
- **현재**: `UExRunnerBuffComponent` + `FExBuffDefinition` (Struct)
- **목표**: `UExBuffComponent` → `ExCore/Components`, `FExBuffDefinition` → `ExCore/Struct`
- **결합 근거**: `UExRunnerMovementComponent`×1, `UExRunnerInputComponent`×1 (= 버프 효과의 *적용 대상*)
- **작업**:
  - Buff 컴포넌트는 **수명/우선순위/잔여시간만 관리하고 GameplayTag 이벤트를 브로드캐스트**하도록 책임 한정.
  - "속도 변경" 등 실제 효과 적용은 러너 쪽이 이벤트를 구독해 처리하도록 역전(현 코드가 이미 GameplayTag 이벤트 적극 사용 → 정합).
- **난이도**: 중간

### 1.3 Rhythm & Music (BeatSync) 시스템
- **현재**: `UExBeatSyncComponent` + `FExBeatSyncSettings`
- **목표**: `UExBeatSyncComponent` → `ExCore/Components` (`UExMusicManagerSubsystem`이 이미 ExCore에 있으므로 인접 배치)
- **결합 근거**: `UExRunnerConfig`×1(설정 로드), `UExObstacleManager`×1(비트마다 장애물 스폰 호출)
  - ⚠️ **정정**: 기존 계획서 v1의 "그대로 이동" 서술은 부정확. 위 두 결합 때문에 **그대로 이동 불가**, 디커플링 필요.
- **작업**:
  - 설정(BPM 등)은 범용 소스(또는 파라미터)로 주입받도록 변경.
  - 비트 발생 시 **"비트 틱 이벤트"만 송출** → 장애물 스폰 같은 게임플레이 반응은 러너가 구독.
- **난이도**: 중간

### 1.4 Rule & Objective 시스템 *(아키텍처 가치 최고)*
- **현재**: `UExRunnerRuleManagerComponent` + `UExRunnerRuleBase` + 구체 룰(`_DistanceGoal`, `_FallDeath`, `_Timer`) + `UExRunnerRuleConfig`(Preset) + `EExRunnerGameOverReason`
- **목표**: `UExRuleManagerComponent` + `UExRuleBase` → **신설** `ExCore/Rules`
- **결합 근거 (최다)**: Manager→`AExRunnerGameMode`×6, `AExRunnerPlayerState`×2, `AExRunnerGameState`×1, `UExRunnerRuleConfig`×1 / RuleBase→`AExRunnerGameMode`×1
- **작업**:
  - Manager의 `AExRunnerGameMode` 캐싱을 **`AExGameModeBase`로 재타게팅**(결합도가 가장 높아 손이 가장 많이 감).
  - 구체 룰 분리: **`_Timer`는 범용** → Core 예시로 이관 / **`_DistanceGoal`·`_FallDeath`는 러너 성격** → Feature 잔류.
  - `EExRunnerGameOverReason` → 범용 종료 사유 enum으로 일반화.
  - **[설계 유지]** 기존처럼 DataAsset(Rule Config)을 ManagerComponent에 꽂는 방식 유지.
- **난이도**: 중간~높음

### 1.5 Match & Lobby UI (MVVM) 시스템
- **현재**: `UExLobbyMatchViewModel`, `UExRunnerMatchViewModel` (+ `UExRunnerFadeOverlayWidget`)
- **목표**: `UExLobbyViewModelBase`, `UExMatchViewModelBase` → `ExCore/UI/ViewModels`
- **결합 근거**:
  - LobbyMatch → `UExRunnerConfig`×4 (+ `UExOnlineSubsystem`/`FExMatch`는 ExNetwork 타입 = 허용)
  - RunnerMatch → `AExRunnerGameState`×2, `AExRunnerPlayerState`×2, `UExRunnerFadeOverlayWidget`×2
- **작업**:
  - LobbyMatch: 매치 설정을 `UExRunnerConfig`가 아닌 범용 인터페이스/베이스에서 읽도록 분리.
  - RunnerMatch: GameState/PlayerState 참조를 베이스 타입으로 일반화. `MatchPhase` 데이터는 이미 `AExGameModeBase::SetMatchPhase`로 Core에 존재. FadeOverlay 위젯도 Core 위젯으로 추출 가능.
- **난이도**: 중간

### 1.6 Path Following 시스템 *(v2 신규 후보 — 기존 계획서 누락)*
- **현재**: `UExPathManager` + `FExPathSegment` (경로 누적거리 → 트랜스폼 변환, 곡선 트랙 지원)
- **목표**: `UExPathFollowComponent`(가칭) → `ExCore/Components`
- **결합 근거**: `AExRunnerGameState`×2 (자기 참조 16 — 자기완결적)
- **재사용처**: 레이싱, 온레일 슈터, 디펜스 경로 등 "경로 추종" 장르 전반.
- **작업**: GameState 거리 의존을 일반화하고, 트레드밀 의미를 걷어낸 "경로+거리→트랜스폼" 순수 기능으로 추출.
- **난이도**: 중간
- **비고**: 즉시 필요하지 않으며, 경로 추종 장르 GameFeature 계획이 생길 때 진행 권장.

---

## 2. 승격 제외 — Feature(ExRunnerPlay) 유지 (🔴)

러너 게임플레이의 본질이거나 베이스가 이미 Core에 있는 Feature 서브클래스:

| 시스템 | 사유 |
|---|---|
| `AExFloorChunk`, `UExChunkSpawner` | 무한 바닥(트레드밀) 본질 |
| `UExObstacleManager`, `UExObstacleDefinition`, `UExObstacleSpawnStrategy`(+Climb/Gap/Slide/WallRun), `IExObstacleInterface` | 러너 장애물 시스템 |
| `UExRunnerItemManager` | 러너 전용 *배치* 로직 (베이스 `UExItemSpawnManagerBase`는 이미 Core) |
| `UExRunnerMovementComponent`, `FLayeredMove_LaneCorrection` | 오토런/레인/트레드밀 이동 |
| `UExRunnerInputComponent`, `UExRunnerInputStrategy`(+AutoRun/AutoButtonRun/Manual) | 러너 입력 매핑 (베이스 `UExInputComponentBase`는 이미 Core). Strategy *패턴* 자체는 범용이나 구체 전략은 러너 |
| `AExRunnerGameMode`/`GameState`/`PlayerState` | 이미 Core 베이스(`AExGameModeBase` 등) 상속한 정상적인 Feature 서브클래스 |
| `UExRunnerConfig`, `UExRunnerItemSpawnTable`, `UExRunnerRuleConfig` | 러너 데이터 에셋 |
| `UExRunnerCheatExtension`, `FExRunnerDebuggerCategory` | 러너 디버그 (베이스는 이미 Core) |

> 단, 이 🔴 시스템들은 §1의 승격된 범용 시스템을 *사용하는* 쪽으로 리팩토링됩니다 (예: 러너가 `ExBuffComponent`를 구독).

---

## 3. 디버그 도구 별도 처리 (🐞)

- **`UExRunnerPositionSyncComponent`**: 서버 권위 위치(`ServerAuthLocation`)를 복제받아, 클라이언트에서 **빨간 캡슐 + 오차 선/텍스트(`DrawDebug*`)로 시각화**하는 **디버그 컴포넌트**. 위치를 보정·적용하는 로직은 없음.
- **처리**: `ExFrameWork_Guidelines.md` §1.11(디버깅 로직의 독립성)에 따라 **프로덕션 범용 시스템 승격 대상이 아님**.
  - 그대로 ExRunnerPlay에 두어도 무방.
  - 굳이 일반화한다면 "서버-클라 위치 오차 시각화"는 장르 무관이므로 `ExCore/Debug`에 디버그 유틸(예: `UExNetPositionDebugComponent`)로 분리 가능 — 단 이는 **선택적 디버그 도구 일반화**이며 본 마이그레이션과 성격이 다름.

---

## 4. 제약 및 공통 작업 패턴

- ✅ **의존성 사전 확보됨**: `ExCoreRuntime.Build.cs`가 이미 `ExNetworkRuntime`·`Mover`에 의존하고, `ExGameFlowSubsystem`이 `UExOnlineSubsystem`을 사용 중 → 로비/매치 VM 이관에 추가 의존성 문제 없음.
- ⚠️ **공통 디커플링 패턴**: 거의 모든 후보가 **Runner GameMode/State 직접 참조**로 결합되어 있음. 핵심 작업은 ①베이스 클래스(`AExGameModeBase`/`AExGameStateBase`) 재타게팅 + ②직접 호출을 GameplayTag/델리게이트 이벤트로 역전.
- ⚠️ **의존성 방향 준수**: 이관 후 ExCore는 ExRunnerPlay를 절대 참조하지 않아야 함 (§4.1). 러너 고유 반응은 모두 Feature 쪽 구독으로.

---

## 5. 권장 작업 순서 (Phase)

저결합·고가치 순으로 재정렬 (기존 v1 순서에서 변경):

- **Phase 1 — 저결합 컴포넌트 워밍업**: `ExStatComponent`(§1.1), `ExBuffComponent`(§1.2) 이관 및 이벤트 역전. (이관 절차/빌드 파이프라인 검증)
- **Phase 2 — 리듬**: `ExBeatSyncComponent`(§1.3) 디커플링 후 이관.
- **Phase 3 — 규칙(고가치·고결합)**: `ExCore/Rules` 신설, `ExRuleManagerComponent`/`ExRuleBase` 추상화 및 GameMode 재타게팅(§1.4).
- **Phase 4 — UI 뷰모델**: `ExLobbyViewModelBase`/`ExMatchViewModelBase` 이관, `ExRunnerConfig` 의존 분리(§1.5).
- **Phase 5 — (조건부) Path Following**: 경로 추종 장르 계획이 확정되면 `ExPathFollowComponent` 추출(§1.6).
- **Phase 6 — 통합 검증**: ExRunnerPlay 게임모드/폰이 이관된 ExCore 시스템을 사용하도록 리팩토링 + 빌드/PIE 검증.

---

## 변경 이력

| 날짜 | 버전 | 내용 |
|------|------|------|
| (초안) | v1.0 | 이관 대상 5종(LobbyMatch/RunnerMatch VM, Rule, Buff, Stat, BeatSync) 및 Phase 1~5 정의 |
| 2026-06-22 | v2.0 | 현재 코드 기준 전면 재검증. 결합도 측정·등급(🟡/🔴/🐞) 도입. 신규 후보 `PathManager`(§1.6) 추가. `PositionSync`는 디버그 도구로 판명되어 후보 제외(§3). BeatSync "그대로 이동" 서술 정정, Stat의 `ExPlayerStatsViewModel` 역할 분담 명시. ExCore↔ExNetwork 의존 사전 확보 확인. 우선순위 재정렬 |
