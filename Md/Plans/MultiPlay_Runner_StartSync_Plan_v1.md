# 멀티플레이 Runner 게임 시작 동기화 Plan

> **목적:** 멀티플레이 환경에서 모든 플레이어가 동시에 게임을 시작하도록 보장하는 안전한 동기화 구조 설계.
> **상태:** 설계 완료 — 구현 대기 중
> **작업 분류:** ExCore + ExRunnerPlay 양 모듈 변경
> **관련 문서:** `Md/Architecture/ExCore/ExFrameWork_Multiplayer_Flow_Architecture.md`, `Md/Bug/MultiPlay/Bug_Mover_Multiplayer_Sync_Error.md`

---

## 1. 문제 정의 (Problem Definition)

### 1.1 현재 동작의 문제점

현재 `AExGameModeBase::HandleStartingNewPlayer_Implementation`은 첫 플레이어 접속 즉시 `CheckAndStartMatch`를 호출하며, `bAutoStartOnReady=true`인 `AExRunnerGameMode`는 즉시 `Match_Playing`으로 전환된다. 이로 인해 다음 문제가 발생한다.

- `UExRunnerMovementComponent`는 매치 단계와 무관하게 `BeginPlay` 시점부터 `MoverComp->QueueNextMode(Walking)`로 강제 진입하고, `ProduceInput_Implementation`이 무조건 전진 입력을 주입한다.
- `UExRunnerInputComponent`의 모든 `Request*` 함수가 매치 단계와 무관하게 동작하여 키 입력이 즉시 캐릭터 행동으로 이어진다.
- FloorChunk 스폰과 Player Pawn 스폰의 순서 보장이 없다. 즉 Pawn이 먼저 스폰되어 빈 공간에 떨어지거나, 클라이언트의 결정론적 청크 재생성이 늦어져 비동기 시작이 발생한다.
- `CheckAllPlayersReady()`는 단순 `HasClientLoadedCurrentWorld()`만 검사하며, Pawn Possess와 Mover 바인딩까지는 검증하지 않는다.


### 1.2 매치 단계 태그 활용 미흡

`ExMatchTags`에는 다음 4개 단계가 이미 정의되어 있으나, 실제 게이팅 로직이 부재하다.

| 태그 | 정의 여부 | 현재 활용 |
|---|---|---|
| `Match.WaitingForPlayers` | 정의됨 | GameState 초기값으로만 존재, 게이팅 미적용 |
| `Match.Countdown` | 정의됨 | 사용처 없음 (Dead Tag) |
| `Match.Playing` | 정의됨 | 매치 시작 트리거로만 활용 |
| `Match.PostMatch` | 정의됨 | 정상 동작 |

### 1.3 이전 멀티플레이 버그 경험

`Md/Bug/MultiPlay/Bug_Mover_Multiplayer_Sync_Error.md`의 교훈에 따라 다음 원칙을 본 설계에 반영한다.

- Mover 시스템은 양쪽이 안정적으로 준비된 상태에서만 동작해야 한다. 입력 누락이나 권한 불일치는 즉각적인 Desync로 이어진다.
- 동적 액터(FloorChunk 등)는 Pawn 스폰 이전에 안정적으로 배치되어야 한다.
- 거리/위치 동기화는 매 프레임 양측에서 공통 계산되어야 하며, 한쪽에서만 갱신하면 안 된다.

---


## 2. 설계 목표 (Design Goals)

1. **이동/입력의 절대 차단:** `Match_Playing` 이전에는 모든 이동 입력과 키 입력이 무시되어야 한다.
2. **순서 보장:** "FloorChunk 사전 스폰 → 모든 플레이어 Pawn 스폰 → 카운트다운 → 동시 시작" 순서를 엄격히 보장한다.
3. **동시 시작:** 어떤 플레이어도 다른 플레이어보다 먼저 출발하지 않는다. GameState 단일 진실 원천(Single Source of Truth) 기반으로 모든 클라이언트가 동시에 게이트가 열린다.
4. **싱글/멀티 일관성:** 싱글플레이(StandAlone)에서도 동일한 흐름과 카운트다운을 적용한다. 분기 로직을 최소화한다.
5. **기존 아키텍처 존중:** ExCore-ExRunnerPlay 의존성 방향, Strategy/Subsystem 등 기존 패턴을 유지한다.

---

## 3. 핵심 설계 원칙 (Core Architecture)

### 3.1 단일 진실 원천 (Single Source of Truth)

- 서버 권한 `AExRunnerGameMode`가 `SetMatchPhase`를 통해 매치 단계를 진행시킨다.
- 모든 클라이언트는 `AExGameStateBase::CurrentMatchPhase` (Replicated) 를 폴링하여 이동/입력 게이트 상태를 판단한다.
- 게이트 판정은 inline 정적 헬퍼(`ExMatchPhaseHelper`)를 단일 진입점으로 사용하여 중복 코드를 제거한다.

### 3.2 의존성 방향 준수

가이드라인 4.1에 따라 ExCore는 ExRunnerPlay를 참조하지 않는다. 본 설계에서 `RunnerConfig`의 매치 흐름 설정은 다음 구조로 추상화된다.

- ExCore의 `AExGameModeBase`는 가상 함수(`GetExpectedPlayerCount` 등)로 인터페이스만 노출
- ExRunnerPlay의 `AExRunnerGameMode`가 오버라이드하여 `UExRunnerConfig::MatchFlow` 값을 반환


### 3.3 매치 시작 시퀀스 (3-Phase)

```
┌─────────────────────────────────────────────────────────────────┐
│ Phase 1: Match.WaitingForPlayers                                │
├─────────────────────────────────────────────────────────────────┤
│ 서버 BeginPlay                                                  │
│   └─ Runner 전용: PrewarmRunnerWorld() 실행                     │
│        └─ ChunkSpawner 초기 N개 FloorChunk 사전 스폰            │
│   └─ MaxWaitForPlayersSeconds 타이머 시작                       │
│                                                                 │
│ PostLogin (플레이어 N명 반복)                                   │
│   └─ Late Join 검사: CurrentMatchPhase != WaitingForPlayers     │
│        ├─ true → Client_ShowLateJoinPopup() RPC, Pawn 스폰 차단 │
│        └─ false → Pawn 스폰 진행                                │
│   └─ ChoosePlayerStart: 레인 슬롯 할당 (우→중→좌)                │
│        └─ PlayerStart Transform + Y/Lane 오프셋 적용             │
│   └─ Pawn 스폰 → BeginPlay                                      │
│        └─ UExRunnerMovementComponent.TryInitializeMover() 성공  │
│             └─ Server_NotifyRunnerReady() RPC                   │
│                  └─ PlayerState.bIsRunnerReady = true            │
│                       └─ CheckAndStartMatch() 호출              │
│                                                                 │
│ CheckAndStartMatch (AND 조건 검사)                              │
│   ├─ 모든 PC HasClientLoadedCurrentWorld()                      │
│   ├─ 모든 PlayerState.bIsRunnerReady                            │
│   ├─ ActivePlayer 수 == ExpectedPlayerCount                     │
│   └─ ChunkSpawner Prewarm 완료                                  │
│        ↓ 모두 만족 또는 타임아웃 발생                            │
│   SetMatchPhase(Match.Countdown)                                │
└─────────────────────────────────────────────────────────────────┘
```


```
┌─────────────────────────────────────────────────────────────────┐
│ Phase 2: Match.Countdown                                        │
├─────────────────────────────────────────────────────────────────┤
│ 서버: 1초 간격 타이머 시작 (CountdownDurationSeconds)            │
│   └─ 매 초마다 GameState.CountdownSecondsRemaining 감소         │
│        └─ Replicated → 클라이언트 OnRep_CountdownChanged 발동   │
│                                                                 │
│ 이 단계에서도 게이트는 닫혀 있음:                                │
│   - MovementComponent: ProduceInput에서 Zero 입력 주입          │
│   - InputComponent: 모든 Request* 함수가 조기 return            │
│                                                                 │
│ 카운트다운 0 도달:                                              │
│   └─ SetMatchPhase(Match.Playing)                               │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ Phase 3: Match.Playing                                          │
├─────────────────────────────────────────────────────────────────┤
│ OnMatchStarted_Implementation (기존 흐름 그대로 유지)            │
│   └─ StartRunnerGame() — BGM, 시드 검증, Rule 활성화 등          │
│                                                                 │
│ 모든 클라이언트의 OnRep_MatchPhase가 동시에 발동:                │
│   - MovementComponent 게이트 해제 → 전진 입력 정상화            │
│   - InputComponent 게이트 해제 → 키 입력 수용                   │
└─────────────────────────────────────────────────────────────────┘
```

---


## 4. 컴포넌트별 책임 분담 (Responsibility Matrix)

| 컴포넌트 | 모듈 | 책임 |
|---|---|---|
| **`AExGameModeBase`** | ExCore | 매치 단계 권한 관리, AND 조건 검증, 카운트다운 타이머 운영, Late Join 검사. 가상 함수로 설정값 인터페이스 제공 |
| **`AExGameStateBase`** | ExCore | `CurrentMatchPhase` 복제(기존), `CountdownSecondsRemaining` 신규 복제, `IsMatchActive()` 헬퍼 노출 |
| **`AExPlayerStateBase`** | ExCore | `bIsRunnerReady` 신규 복제 — Pawn 측 준비 완료 보고 채널 |
| **`ExMatchPhaseHelper`** | ExCore | inline 정적 헬퍼 — `IsMatchActive(WorldContext)` 단일 진입점 제공 |
| **`AExRunnerGameMode`** | ExRunnerPlay | `PrewarmRunnerWorld()` 분리, `ChoosePlayerStart` 오버라이드(레인 분산), `Client_ShowLateJoinPopup` RPC 발신, 설정값 가상 함수 오버라이드 |
| **`UExRunnerConfig`** | ExRunnerPlay | `FExMatchFlowSettings MatchFlow` 신규 멤버 — 매치 흐름 파라미터 보유 |
| **`UExRunnerMovementComponent`** | ExRunnerPlay | `ProduceInput`/`TickComponent` 초입에서 게이트 체크, `Server_NotifyRunnerReady` RPC 발신 |
| **`UExRunnerInputComponent`** | ExRunnerPlay | 모든 `Request*` 함수 초입에서 게이트 체크 |

---


## 5. 신규 파일 명세

### 5.1 `FExMatchFlowSettings` 구조체

**위치:** `Plugins/GameFeatures/ExRunnerPlay/Source/ExRunnerPlayRuntime/Struct/Modes/FExMatchFlowSettings.h`
**역할:** 매치 시작 흐름을 제어하는 설정값을 묶은 USTRUCT. `UExRunnerConfig`의 멤버로 포함되어 데이터 드리븐 방식으로 관리된다.
**필드 의도:**
- 기대 플레이어 수 — 매치 시작 조건의 핵심 기준. 1이면 싱글플레이 동작.
- 카운트다운 지속 시간 — 0이면 즉시 시작, 양수면 해당 초만큼 대기 후 시작.
- 최대 대기 시간 — 모든 플레이어가 준비되지 못해도 이 시간 경과 후 도착한 인원만으로 강제 시작.
- 레인 슬롯 우선순위 — 입장 순서별 레인 인덱스 배열(기본: `[+1, 0, -1]` 즉 우→중→좌).

**기본값:**
- `ExpectedPlayerCount`: 1 (싱글플레이 호환)
- `CountdownDurationSeconds`: 3.0
- `MaxWaitForPlayersSeconds`: 30.0
- `LaneSlotOrder`: `{+1, 0, -1}` (TArray<int32>)

**가이드라인 준수:**
- 가이드라인 1.5: `Struct/Modes/` 하위 배치
- 가이드라인 1.3: 모든 필드 `UPROPERTY(EditAnywhere)` 노출
- 가이드라인 1.2: USTRUCT 명명 `FEx` 접두사


### 5.2 `ExMatchPhaseHelper` 정적 헬퍼

**위치:** `Plugins/GameFeatures/ExCore/Source/ExCoreRuntime/Util/Match/ExMatchPhaseHelper.h`
**역할:** 매치 단계 게이트 판정을 위한 inline 정적 헬퍼. WorldContext에서 GameState를 얻어 매치 단계를 판정하는 모든 코드를 단일 진입점으로 통합한다.

**API (개념):**
- `IsMatchActive(WorldContext)` — `CurrentMatchPhase == Match_Playing`일 때만 true
- `IsBeforeMatchStart(WorldContext)` — Waiting 또는 Countdown 단계일 때 true
- `IsMatchEnded(WorldContext)` — PostMatch 단계일 때 true
- `GetCurrentMatchPhase(WorldContext)` — 현재 단계 태그 직접 반환

**구현 원칙:**
- 모든 함수는 inline static (헤더 전용)
- WorldContext nullptr 또는 GameState 미존재 시 안전하게 false 반환
- 가이드라인 1.5: `Util/Match/` 하위 배치

**왜 이 헬퍼가 필요한가:**
- MovementComponent, InputComponent, UI ViewModel 등 다수의 호출자가 동일한 판정 로직을 필요로 한다.
- 중복 코드는 향후 매치 단계 변경 시 동기화 누락 버그의 온상이 된다.
- 단일 진입점은 추후 디버깅/로깅 훅을 추가하기 쉽다.

---


## 6. 기존 파일 변경 명세

### 6.1 `UExRunnerConfig` (ExRunnerPlay)

**변경 내용:** `FExMatchFlowSettings MatchFlow` 멤버 신규 추가 (`UPROPERTY(EditAnywhere)`).
**영향 범위:** 기존 필드는 변경하지 않으므로 기존 DataAsset 인스턴스의 호환성 유지.

### 6.2 `AExGameModeBase` (ExCore)

**신규 가상 함수 (인터페이스용 — 기본값 반환):**
- `GetExpectedPlayerCount()` — 기본값 1
- `GetCountdownDuration()` — 기본값 0초
- `GetMaxWaitForPlayersSeconds()` — 기본값 30초
- 이 3개 함수는 ExCore가 ExRunnerPlay를 모르고도 매치 흐름을 운영할 수 있게 하는 핵심 추상화이다.

**`CheckAndStartMatch()` 강화:**
- AND 조건으로 다음 4가지 검증:
  1. `CurrentMatchPhase == Match_WaitingForPlayers`
  2. 모든 PlayerController가 `HasClientLoadedCurrentWorld() == true`
  3. 모든 PlayerState의 `bIsRunnerReady == true`
  4. ActivePlayer 수가 `GetExpectedPlayerCount()` 이상
- 모두 만족하면 `OnAllPlayersReady()` 호출 (보호 가상 함수)

**`OnAllPlayersReady()` 신규 보호 가상 함수:**
- 기본 동작: `StartCountdown()` 호출
- 하위 클래스에서 추가 사전 작업 가능 (예: Runner는 ChunkSpawner Prewarm 검증)


**`StartCountdown()` 신규 함수:**
- `GetCountdownDuration()` 값을 GameState의 `CountdownSecondsRemaining`에 세팅
- `SetMatchPhase(Match_Countdown)` 호출
- 1초 간격 반복 타이머 등록 → 매 초마다 카운트다운 감소 + GameState 갱신
- 0 도달 시 `FinishCountdown()` 호출

**`FinishCountdown()` 신규 함수:**
- 카운트다운 타이머 정리
- `SetMatchPhase(Match_Playing)` 호출 → 기존 `OnMatchStarted_Implementation` 흐름 자동 트리거

**`MaxWaitForPlayersSeconds` 타임아웃 처리:**
- `BeginPlay`에서 `GetMaxWaitForPlayersSeconds()` 값으로 단발 타이머 등록
- 타이머 만료 시점에 도착한 인원이 1명 이상이면 강제 `OnAllPlayersReady()` 호출
- 0명이면 매치 자체를 종료 처리 검토 (별도 로깅 후 ensure)

**`ServerNotifyPlayerReady(APlayerController*)` 신규 RPC (Server, Reliable):**
- MovementComponent의 Mover 바인딩 완료 시 호출됨
- 해당 PC의 PlayerState에서 `bIsRunnerReady = true` 설정
- 즉시 `CheckAndStartMatch()` 재호출

**`PostLogin(APlayerController*)` 오버라이드:**
- 기존 동작 유지 + Late Join 검사 추가:
  - `CurrentMatchPhase != Match_WaitingForPlayers`인 경우:
    - 해당 PC에게 Client RPC로 팝업 표시 요청 (Runner 측에서 구현된 RPC 호출)
    - `HandleStartingNewPlayer` 호출 차단 (Pawn 스폰 방지)
    - 가이드라인 1.7: `ensureMsgf`로 비정상 입장 로깅


**`Logout(AController*)` 오버라이드:**
- 카운트다운 진행 중 Disconnect 발생 시 처리
- 결정사항에 따라 남은 인원으로 카운트다운 계속 진행 (별도 중단 로직 없음)
- 단, PlayerState 정리만 정상적으로 수행 (Super 호출)

### 6.3 `AExGameStateBase` (ExCore)

**신규 멤버:**
- `int32 CountdownSecondsRemaining` (Replicated) — 카운트다운 표시용 정수 초
- `FOnCountdownChanged` (Multicast Dynamic 델리게이트) — `OnRep_CountdownSeconds`에서 broadcast

**신규 헬퍼:**
- `bool IsMatchActive() const` (BlueprintPure) — `CurrentMatchPhase == Match_Playing` 반환
- `ExMatchPhaseHelper`와 동일한 의미이지만 GameState 인스턴스에서 직접 접근 시 편의용

**`OnRep_CountdownSeconds()` 콜백:**
- `OnCountdownChanged.Broadcast(CountdownSecondsRemaining)` 호출
- UI ViewModel이 구독하여 카운트다운 표시 갱신 (UI 작업은 별도 후속 작업)

### 6.4 `AExPlayerStateBase` (ExCore)

**신규 멤버:**
- `bool bIsRunnerReady` (Replicated, 기본 false)

**의도:** Pawn 측에서 Mover 바인딩 완료 등 "이 플레이어는 게임 시작 가능 상태"임을 서버에 보고하는 채널. GameMode의 `CheckAndStartMatch`가 이 플래그를 AND 조건으로 검증.


### 6.5 `AExRunnerGameMode` (ExRunnerPlay)

**`PrewarmRunnerWorld()` 신규 함수:**
- `BeginPlay` 후반에 호출
- 기존 `StartRunnerGame()` 내부의 ChunkSpawner 초기 N개 스폰 로직을 이 함수로 분리/이관
- 구체적으로 다음 단계 수행:
  1. PathManager 초기화
  2. SharedTrackSeed 생성 (`FMath::Rand()`)
  3. ObstacleManager/ItemManager 시드 초기화
  4. ChunkSpawner의 `InitializeSpawner()` 호출 → 초기 청크 N개 스폰
- 이 함수가 끝나야 비로소 플레이어 Pawn 스폰이 안전한 상태가 됨

**`StartRunnerGame()` 정리:**
- 위 4단계가 `PrewarmRunnerWorld()`로 이관됨
- 남은 책임: BGM 시작, Rule 활성화, 기타 매치 시작 시 동작
- 즉 `StartRunnerGame`은 "Match_Playing 진입 시 활성화" 책임만 가짐

**`ChoosePlayerStart_Implementation` 오버라이드:**
- PlayerStart 액터를 기본으로 선택 (Super 호출)
- 액터 부재 시 `ensureMsgf` 발동 + (0,0,0) fallback Transform 반환 (게임 진행은 유지)
- 서버 멤버 `int32 NextLaneSlotIndex` (기본 0) 를 사용하여 다음 레인 슬롯 결정
- `RunnerConfig.MatchFlow.LaneSlotOrder[NextLaneSlotIndex]` 값을 가져와 레인 인덱스 도출
- `NextLaneSlotIndex >= LaneSlotOrder.Num()`인 경우 `ensureMsgf` 발동 ("3인 이상 매치는 미지원, 가변 레인 시스템 도입 필요")
- 도출된 레인 인덱스 × LaneWidth만큼 Y축 오프셋 적용한 Transform 반환


**레인 폭(LaneWidth) 결정 우선순위:**
1. ChunkSpawner의 활성 청크가 있으면 첫 청크의 `GetFloorBounds().GetSize().Y / 3.0`
2. 위가 실패하면 `RunnerConfig.Movement.LaneWidth` 사용 (정적 fallback)

`PrewarmRunnerWorld`가 `PostLogin`보다 먼저 실행되므로 1번 경로가 정상 작동할 것으로 기대된다.

**`ChoosePlayerStart` 후처리:**
- 성공적으로 Transform 반환 시 `NextLaneSlotIndex++`
- Pawn 스폰 실패 시 인덱스 복구 로직 검토 (구현 시 결정)

**가상 함수 오버라이드 (ExCore 인터페이스 구현):**
- `GetExpectedPlayerCount()` → `RunnerConfig->MatchFlow.ExpectedPlayerCount`
- `GetCountdownDuration()` → `RunnerConfig->MatchFlow.CountdownDurationSeconds`
- `GetMaxWaitForPlayersSeconds()` → `RunnerConfig->MatchFlow.MaxWaitForPlayersSeconds`
- 각 함수는 `RunnerConfig.IsValid()` 체크 후 기본값(Super 호출) fallback

**`Client_ShowLateJoinPopup()` 신규 Client RPC:**
- `AExGameModeBase::PostLogin`의 Late Join 검사에서 호출됨 (실제로는 RunnerGameMode에서 오버라이드된 PostLogin이 호출)
- 클라이언트 측에서 `FExPopupDescriptor`를 구성하여 `UExPopupWidget` 표시
- 팝업 구성: 단일 "확인" 버튼, 본문 "매치가 이미 시작되어 입장할 수 없습니다", 결과 무관 단순 닫기
- 로비 복귀나 Spectator 처리는 현재 범위 외 (추후 검토)


### 6.6 `UExRunnerMovementComponent` (ExRunnerPlay)

**`ProduceInput_Implementation` 변경:**
- 함수 초입에 게이트 체크: `ExMatchPhaseHelper::IsMatchActive(this)` 가 false인 경우
  - `FCharacterDefaultInputs`에 다음 값을 채워서 종료:
    - `DirectionalInput = FVector::ZeroVector` (전진 입력 차단)
    - `OrientationIntent = ` 현재 PathManager 기준 정면 방향 유지 (캐릭터 자세 안정)
    - `SuggestedMovementMode = ` 비할당 (기존 Walking 유지)
  - 기존 ForwardDir 계산 / Pure Pursuit / Joystick 누적 등 로직은 모두 스킵
- 이렇게 하면 Mover는 "정지 의사" 입력을 수신하여 자연스럽게 그 자리에 서 있음

**`TickComponent` 변경:**
- 기존 거리 갱신 로직(CurrentPathDistance 갱신)은 게이트 무관 유지 — 카운트다운 중에도 거리는 0 또는 매우 작음
- `UpdateLanePosition` 호출은 게이트 체크 — 카운트다운 중에는 레인 보간 차단
- `UpdateCharacterRotation` 호출은 유지 — 카운트다운 중에도 캐릭터가 PathManager 정면을 향하도록 시각적 안정 유지
- PlayerState 거리 동기화(`UpdatePathDistance`)는 그대로 유지 — 항상 0에 가까운 값이 동기화될 뿐 문제 없음

**`TryInitializeMover()` 변경:**
- 성공적으로 Mover 바인딩이 완료된 시점 마지막에 `Server_NotifyRunnerReady()` RPC 호출 추가
- 이 RPC는 서버 측에서 `AExGameModeBase::ServerNotifyPlayerReady(GetOwningPlayerController())` 호출로 위임

**`Server_NotifyRunnerReady()` 신규 RPC (Server, Reliable):**
- 서버에서 GameMode에 자신의 PC 전달 → PlayerState.bIsRunnerReady = true → CheckAndStartMatch 트리거
- Owner Pawn의 PlayerController를 통해 호출되므로 RPC 권한 검증 자연스럽게 통과


### 6.7 `UExRunnerInputComponent` (ExRunnerPlay)

**게이트 적용 함수 (모두 동일 패턴):**
- `RequestJumpAction(bool)`
- `RequestSlideAction(bool)`
- `RequestSprintAction(bool)`
- `RequestMoveAction(FVector2D)`
- `RequestLookAction(float)`
- `RequestLaneChange(int32)`

**각 함수 초입 게이트 패턴:**
- `if (!ExMatchPhaseHelper::IsMatchActive(this)) return;`
- 단일 헬퍼 호출로 모든 입력 경로가 일관되게 차단됨

**Strategy 객체에서의 처리:**
- Strategy 클래스(`ExRunnerInputStrategy_AutoButtonRun` 등)는 직접 호출하지 않으므로 별도 게이트 불필요
- 모든 진입점이 `UExRunnerInputComponent::Request*`를 거치도록 통일되어 있음
- 만약 Strategy가 직접 델리게이트를 broadcast하는 경로가 발견되면 해당 위치도 게이트 추가 (구현 시 확인)

**입력 모드 전환(`SetInputMode`)은 게이트 미적용:**
- 매치 단계와 무관하게 UI에서 입력 모드를 미리 선택할 수 있어야 함
- 게임 시작 전에 AutoRun/Manual 모드 선택은 정상 동작

---


## 7. 시나리오별 동작 정리

### 7.1 정상 시나리오 (3인 매치)

1. 서버 BeginPlay → `Match_WaitingForPlayers`, PrewarmRunnerWorld 실행 → FloorChunk N개 스폰
2. 1번째 PC 접속 → ChoosePlayerStart에서 레인 +1(우) 할당 → Pawn 스폰 → Mover 바인딩 → Ready 신호
3. 2번째 PC 접속 → 레인 0(중앙) 할당 → Pawn 스폰 → Ready 신호
4. 3번째 PC 접속 → 레인 -1(좌) 할당 → Pawn 스폰 → Ready 신호 → 모든 조건 충족
5. `OnAllPlayersReady` → `StartCountdown` → GameState.CountdownSecondsRemaining = 3
6. 3, 2, 1 카운트다운 (이 동안 모든 캐릭터는 자기 자리에서 정지, 입력 무시)
7. 0 도달 → `FinishCountdown` → `SetMatchPhase(Match_Playing)` → 모든 클라이언트 OnRep 동시 발동
8. MovementComponent/InputComponent 게이트 해제 → 모두 동시 출발

### 7.2 싱글플레이 시나리오

- `ExpectedPlayerCount = 1` 설정 시 1번째 PC 접속 즉시 모든 조건 충족
- 카운트다운 동일하게 3초 진행 후 시작
- 게이트 메커니즘은 동일하게 작동 (멀티/싱글 분기 없음)

### 7.3 Late Join 시나리오

- Match_WaitingForPlayers 이후 단계에서 접속 시도
- `PostLogin`에서 `CurrentMatchPhase != Match_WaitingForPlayers` 감지
- 해당 PC에게 `Client_ShowLateJoinPopup` RPC 발신 (단일 "확인" 버튼)
- 서버는 해당 PC의 Pawn을 스폰하지 않음 (HandleStartingNewPlayer 차단)
- 클라이언트는 팝업 확인 후 단순 닫기 (로비 복귀 등 추가 처리 없음)
- 가이드라인 1.7: `ensureMsgf("Late Join attempted at phase %s")`로 로깅


### 7.4 Disconnect 시나리오

- 카운트다운 도중 한 PC가 Disconnect
- `AExGameModeBase::Logout` 호출 → Super 호출만 수행 (PlayerState 정리)
- 카운트다운은 중단 없이 계속 진행 → 남은 인원으로 시작
- 결정사항: "남은 인원만으로 카운트다운 계속 진행"

### 7.5 타임아웃 시나리오

- `MaxWaitForPlayersSeconds` (기본 30초) 경과 후에도 모든 조건 미충족
- 도착한 인원이 1명 이상이면 강제 `OnAllPlayersReady` → 카운트다운 시작
- 도착 인원 0명이면 `ensureMsgf` 발동 후 매치 종료 처리 (드문 케이스)

### 7.6 PlayerStart 부재 시나리오

- 맵에 PlayerStart 액터가 없음
- `ChoosePlayerStart_Implementation`에서 Super 호출 결과가 nullptr
- `ensureMsgf` 발동 ("PlayerStart not found in map %s")
- (0,0,0) Transform에 레인 오프셋만 적용하여 fallback 스폰
- 게임은 진행 가능 (디버그 가능 상태 유지)

### 7.7 3인 초과 매치 시도 시나리오

- `RunnerConfig.MatchFlow.ExpectedPlayerCount > 3` 설정 또는 LaneSlotOrder에 3개 초과 인덱스 설정
- 4번째 PC 접속 시 `ChoosePlayerStart`에서 `NextLaneSlotIndex >= LaneSlotOrder.Num()` 감지
- `ensureMsgf` 발동 ("4인 이상 매치는 미지원, 가변 레인 시스템 도입 필요")
- 가장 마지막 레인 슬롯에 중복 스폰 또는 fallback (구현 시 결정)

---


## 8. 구현 단계 (Implementation Phases)

각 Phase 완료 시 주인님께 보고 후 다음 단계로 진행한다.

### Phase 1 — ExCore 기반 구축

**대상 파일:**
- 신규: `Util/Match/ExMatchPhaseHelper.h`
- 수정: `GameModes/ExGameStateBase.h/cpp` — CountdownSecondsRemaining, IsMatchActive, OnCountdownChanged
- 수정: `Player/ExPlayerStateBase.h/cpp` — bIsRunnerReady

**검증 방법:**
- 컴파일 통과 확인
- 헬퍼 함수 단위 테스트 (GameState 인스턴스 유무에 따른 false 안전 반환)

### Phase 2 — ExCore GameMode 흐름 강화

**대상 파일:**
- 수정: `GameModes/ExGameModeBase.h/cpp`
  - 가상 함수 3종 추가
  - CheckAndStartMatch 강화 (AND 조건)
  - OnAllPlayersReady, StartCountdown, FinishCountdown 추가
  - ServerNotifyPlayerReady RPC 추가
  - PostLogin Late Join 검사 추가
  - MaxWaitForPlayersSeconds 타임아웃 타이머 추가
  - Logout 오버라이드

**검증 방법:**
- PIE Standalone 모드에서 ExpectedPlayerCount=1로 카운트다운 정상 진행 확인
- 로그를 통해 매치 단계 전이 흐름 확인


### Phase 3 — ExRunnerPlay 통합 (Config / GameMode)

**대상 파일:**
- 신규: `Struct/Modes/FExMatchFlowSettings.h`
- 수정: `Data/ExRunnerConfig.h/cpp` — MatchFlow 멤버 추가
- 수정: `GameModes/ExRunnerGameMode.h/cpp`
  - PrewarmRunnerWorld 분리
  - StartRunnerGame 정리
  - ChoosePlayerStart 오버라이드 (레인 분산)
  - 가상 함수 3종 오버라이드
  - Client_ShowLateJoinPopup RPC 추가

**검증 방법:**
- 기존 DataAsset 인스턴스가 MatchFlow 기본값으로 정상 로드되는지 확인
- 1인 PIE에서 레인 0(중앙) 스폰 확인
- ChunkSpawner Prewarm이 BeginPlay에 정상 동작하는지 확인

### Phase 4 — Movement/Input 게이트 적용

**대상 파일:**
- 수정: `Components/ExRunnerMovementComponent.h/cpp` — ProduceInput 게이트, Server_NotifyRunnerReady RPC
- 수정: `Components/ExRunnerInputComponent.cpp` — Request* 함수들 게이트

**검증 방법:**
- 1인 PIE에서 카운트다운 동안 키보드 입력 무시 확인
- 카운트다운 종료 후 정상 전진 확인
- 카운트다운 중 캐릭터가 자리에 정지 상태 유지 확인 (Mover 이상 동작 없음)


### Phase 5 — 멀티플레이 검증 및 버그 문서화

**검증 항목:**
- PIE 2-Client 모드에서 2인 매치 정상 동작 확인
- PIE 3-Client 모드에서 3인 매치 정상 동작 확인 (레인 분산 확인)
- 의도적 Late Join 시도 (Listen Server 후 추가 PC 접속) → 팝업 표시 확인
- 한 PC만 접속한 상태에서 타임아웃 강제 시작 확인
- 카운트다운 중 Disconnect 시 남은 인원으로 진행 확인

**문서화:**
- 검증 중 발견된 멀티플레이 이슈는 `Md/Bug/MultiPlay/` 하위에 즉시 문서화 (가이드라인 3.4)
- 본 Plan 문서를 최종 정리하여 `Md/Architecture/ExCore/` 또는 `Md/Architecture/ExRunnerPlay/`로 이관 검토

---

## 9. 변경하지 않는 것 (Out of Scope)

다음 항목은 본 작업 범위에 포함되지 않으며 기존 동작을 그대로 유지한다.

- `SharedTrackSeed` 기반 클라이언트 결정론적 청크 재생성 로직 (`OnRep_SharedTrackSeed`)
- ChunkSpawner의 활성/풀 관리 메커니즘
- ObstacleManager / ItemManager의 Plan/Realize 분리 구조
- PathManager의 경로 계산 로직
- Rule 시스템의 활성화/비활성화 흐름
- BGM 시작/정지 로직
- UI ViewModel의 카운트다운 표시 (별도 후속 작업)
- 로비 복귀 / Spectator 모드 (Late Join 시 단순 팝업 닫기로 처리)
- 가변 레인 시스템 (3인 초과 매치는 ensure로 차단)

---


## 10. 잠재 리스크 및 대응 (Risk Analysis)

| 리스크 | 대응 방안 |
|---|---|
| Mover가 빈 입력을 받았을 때 오작동 | DirectionalInput=Zero + OrientationIntent 유지로 "정지 의사"를 명시적으로 전달. Mover 공식 가이드 권장 패턴 |
| 클라이언트가 OnRep_MatchPhase 수신 전 한 프레임 미세 진행 | MovementComponent가 Tick마다 폴링하므로 첫 Tick에 즉시 차단. 무시 가능한 수준 |
| ChunkSpawner Prewarm이 PostLogin보다 늦게 실행되는 경우 | BeginPlay 후반에 Prewarm 호출, PostLogin은 그 이후 수신되므로 순서 안전. 만약 PostLogin이 먼저 호출되어도 ChoosePlayerStart의 fallback 경로(RunnerConfig.Movement.LaneWidth)가 작동 |
| 한 플레이어 영원히 로딩 미완료 시 매치 무한 대기 | MaxWaitForPlayersSeconds 타임아웃으로 강제 시작 |
| Late Join 거부 시 클라이언트가 팝업 닫고도 빈 화면 유지 | 결정사항: 로비 복귀는 구현 범위 외. 향후 필요 시 추가 작업 |
| 가이드라인 1.10 (생성자 NewObject 금지) 위반 위험 | 신규 컴포넌트/서브오브젝트 추가 없음. 모든 신규 객체는 BeginPlay 이후 시점에 동작 |
| 기존 멀티플레이 Mover Sync 버그(`Bug_Mover_Multiplayer_Sync_Error.md`) 재발 | 본 설계는 해당 수정사항(레인 동기화 RPC, 거리 갱신 공통화, FloorChunk Stationary)을 모두 보존하며 그 위에 추가 |

---

## 11. 가이드라인 준수 체크리스트

- [x] 1.2 명명 규칙: 모든 신규 클래스/구조체에 Ex 접두사
- [x] 1.3 데이터 드리븐: MatchFlow 모든 필드 UPROPERTY 노출
- [x] 1.4 단일 책임: Helper, Settings 등 책임별 분리
- [x] 1.5 폴더 구조: Struct/Modes/, Util/Match/ 하위 배치
- [x] 1.6 주석/문서: 모든 신규 함수에 Doxygen 주석 작성 예정
- [x] 1.7 검증/체크: 모든 비정상 경로에 ensure/check
- [x] 1.8 서버 권한: 매치 단계 변경은 서버 권한, 복제로 전파
- [x] 1.11 디버깅: 매치 단계 전환 시 UE_LOG 적극 활용
- [x] 3.1 보고/승인: 본 Plan 자체가 사전 보고
- [x] 3.4 멀티플레이 버그: 검증 중 이슈 발생 시 Md/Bug/MultiPlay/ 분리 기록
- [x] 4.1 의존성 방향: ExCore는 ExRunnerPlay를 모르고, 가상 함수 인터페이스만 노출

---

## 12. 승인 요청

주인님, 본 Plan 검토 후 승인해주시면 Phase 1부터 순차적으로 구현에 착수하겠습니다. 각 Phase 완료 시 보고드리고 다음 Phase 진행 승인을 받겠습니다.

수정/추가가 필요한 항목이 있으면 알려주십시오.
