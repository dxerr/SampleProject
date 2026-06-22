# ExNetwork Plugin Phase 4 — Lobby → Game 전환 Plan

> **목적:** 매칭 완료된 Lobby에서 실제 게임 맵으로 전환하는 흐름을 구현한다. Listen Server 호스트가 ServerTravel을 수행하고, 참가 클라이언트는 자동으로 따라오는 표준 UE 멀티플레이 전환 흐름을 완성한다.
> **상태:** 설계 완료 — 구현 승인 대기
> **선행 완료:** Phase 3 — EOS Lobby Quick Match 매칭 성공
> **후속 Plan:** Plan 5 — Matchmaking UI

---

## 1. 작업 범위 (Scope)

### 1.1 본 Phase에서 진행하는 것

- `Strategies/ExListenServerStrategy.h/cpp` — `StartGameSession()` 실제 구현 (ServerTravel)
- `Core/ExOnlineSubsystem.h/cpp` — `StartGame()` 공개 API 추가
- `ExMatchTypes.h` — `EExMatchState`에 `InGame` 상태 추가
- `Events/ExNetEvents.h` — 게임 전환 관련 델리게이트 추가
- `Providers/EOS/ExEOSLobbyProvider.h/cpp` — `GetResolvedConnectString()` 활용하여 접속 주소 획득

### 1.2 본 Phase에서 진행하지 않는 것

- UI 작업 (Plan 5)
- 게임 종료 후 Lobby 복귀 흐름 (Phase 5+ 검토)
- Dedicated Server 전략 구현 (Phase 6+)
- ExRunnerGameMode의 기존 멀티플레이 시작 동기화 흐름 변경 (별도 Plan)

---

## 2. 핵심 설계 원칙

### 2.1 Listen Server의 ServerTravel 흐름

EOS Lobby 기반 Listen Server에서 게임 맵으로 전환하는 표준 흐름:

```
호스트(Server) 관점:
  StartGameSession(MapPath) 호출
      ↓
  UWorld::ServerTravel(MapPath)
      → 호스트가 새 맵으로 이동
      → 연결된 모든 클라이언트 자동으로 따라옴 (UE 표준 동작)

클라이언트 관점:
  서버의 ServerTravel에 의해 자동으로 ClientTravel
  → 별도 코드 불필요 (UE 엔진이 처리)
```

### 2.2 "호스트만 StartGameSession 호출" 원칙

ServerTravel은 **서버 권한**에서만 실행되어야 한다. 클라이언트가 호출하면 무시된다.

```
UExOnlineSubsystem::StartGame()
    ↓
HasAuthority() 또는 IsServer() 확인
    ↓ (서버만)
ExListenServerStrategy::StartGameSession(MapPath)
    ↓
GetWorld()->ServerTravel(MapPath)
```

### 2.3 Lobby 상태와 게임 전환 타이밍

Quick Match 완료 후 바로 StartGame을 호출하는 것이 아니라, **충분한 플레이어가 모인 후** 호스트(서버)가 명시적으로 호출해야 한다.

```
매칭 흐름:
  FindQuickMatch() → Lobby 생성 (Waiting 상태)
      ↓ (다른 플레이어 참가 완료)
  OnMatchFound 브로드캐스트 (Ready 상태)
      ↓ (호스트가 판단)
  StartGame(MapPath) 호출
      ↓
  ServerTravel → 게임 시작
```

Phase 4에서는 **매칭 완료 즉시 자동으로 StartGame을 호출**하는 방식으로 구현하여 흐름을 검증한다. Phase 5 UI에서 "게임 시작" 버튼으로 명시적 호출로 변경 가능하다.

---

## 3. 폴더 구조 변경 (Phase 4 추가분)

```
Source/ExNetworkRuntime/
├── Match/
│   └── ExMatchTypes.h              ← 수정: EExMatchState에 InGame 추가
│
├── Events/
│   └── ExNetEvents.h               ← 수정: OnGameStarted 델리게이트 추가
│
├── Core/
│   ├── ExOnlineSubsystem.h/cpp     ← 수정: StartGame() API 추가
│   └── ...
│
└── Strategies/
    ├── ExListenServerStrategy.h/cpp ← 수정: StartGameSession() 실제 구현
    └── ...
```

신규 파일 생성은 없음. 기존 파일 보강만 진행.

---

## 4. 파일별 수정 명세

### 4.1 `Match/ExMatchTypes.h` — EExMatchState 보강

**추가할 상태:**
- `InGame` — ServerTravel 완료 후 게임 중 상태

**수정 후 전체 상태 흐름:**
```
Idle → Searching → (Creating | Joining) → Waiting → Ready → InGame
```

### 4.2 `Events/ExNetEvents.h` — 게임 전환 델리게이트 추가

**추가할 델리게이트:**
- `FExOnGameStartedDelegate(const FString& MapPath)` — 게임 전환 시작 시 브로드캐스트

### 4.3 `Strategies/ExListenServerStrategy.h/cpp` — StartGameSession 구현

**Phase 4에서 채워지는 핵심 함수:**

`StartGameSession(const FString& MapPath)` 동작:
1. `GetWorld()` 유효성 확인
2. `GetWorld()->IsServer()` 또는 `GetWorld()->GetNetMode() != NM_Client` 확인 — 서버만 실행
3. `GetWorld()->ServerTravel(MapPath + TEXT("?listen"))` 호출
4. `?listen` 옵션: Listen Server로 열기 (클라이언트가 접속 가능하게)
5. UE 엔진이 연결된 모든 클라이언트에게 자동으로 따라오게 함

**`?listen` 옵션의 역할:**
- 이 옵션 없이 ServerTravel하면 단순 로컬 이동
- `?listen` 옵션으로 새 맵에서도 다른 클라이언트가 접속 가능한 서버 상태 유지

**클라이언트 접속 주소 처리:**
- `GetResolvedConnectString()`으로 EOS 접속 주소 획득 (EOS P2P 주소 형식: `EOS.xxx`)
- 클라이언트는 ServerTravel에 의해 자동으로 이 주소로 이동 (별도 구현 불필요)

### 4.4 `Core/ExOnlineSubsystem.h/cpp` — StartGame API 추가

**추가할 공개 API:**

```cpp
// 매칭 완료 후 게임 시작 (호스트만 유효)
UFUNCTION(BlueprintCallable, Category = "ExNetwork|Match")
void StartGame(const FExMatchConfig& Config);

// 게임 전환 시작 시 브로드캐스트
UPROPERTY(BlueprintAssignable, Category = "ExNetwork|Match")
FExOnGameStartedDynDelegate OnGameStarted;
```

**`StartGame()` 내부 흐름:**
1. `GetMatchState() == EExMatchState::Ready` 확인
2. `IsLoggedIn()` 확인
3. `Config.MapPath` 유효성 확인
4. `CurrentMatchState = EExMatchState::InGame` 설정
5. `ListenStrategy->StartGameSession(Config.MapPath)` 호출
6. `OnGameStarted` 브로드캐스트

---

## 5. 구현 단계 (Implementation Steps)

### Step 1 — ExMatchTypes 및 Events 수정

**작업:**
- `EExMatchState`에 `InGame` 추가
- `ExNetEvents.h`에 `FExOnGameStartedDelegate` 추가

**검증:** 빌드 통과

### Step 2 — ExListenServerStrategy::StartGameSession 구현

**작업:**
- `StartGameSession(MapPath)` 실제 구현
- `GetWorld()->ServerTravel()` 호출 (서버 권한 체크 포함)

**검증:** 빌드 통과

### Step 3 — ExOnlineSubsystem::StartGame API 추가

**작업:**
- `StartGame(Config)` 함수 구현
- `OnGameStarted` Dynamic 델리게이트 추가

**검증:** 빌드 통과

### Step 4 — 전체 흐름 검증

**검증 시나리오 (PIE 2-Client):**

```
1. 두 클라이언트 모두 FindQuickMatch() 호출
2. 매칭 완료 (OnMatchFound 브로드캐스트)
3. 서버 인스턴스에서 StartGame(Config) 호출
   (Config.MapPath = "/Game/Maps/RunnerLevel" 또는 기존 맵)
4. ServerTravel 실행
5. 두 클라이언트 모두 새 맵으로 이동 확인
```

**기대 로그:**
```
[ExListenServerStrategy] StartGameSession — MapPath=/Game/Maps/... 에서 ServerTravel 실행
[UExOnlineSubsystem] 게임 전환 시작 — MapPath=/Game/Maps/...
(새 맵에서)
[UExOnlineSubsystem] Initialize 시작. (새 맵의 GameInstance 갱신)
[ExEOSAuthProvider] DeviceId 준비 완료 (EOS_DuplicateNotAllowed) — Connect Login 시작.
[ExEOSAuthProvider] Connect Login 성공
```

---

## 6. 게임 종료 후 Lobby 복귀 (Out of Scope — 추후 검토)

Phase 4에서는 **Lobby → Game 방향만** 구현한다. 게임 종료 후 Lobby로 복귀하는 역방향 흐름은 다음 단계에서 별도 결정한다.

역방향 흐름의 선택지:
- **(A)** 게임 종료 시 Lobby 맵으로 ServerTravel (Lobby 상태 재생성)
- **(B)** 게임 종료 시 메인 메뉴로 이동 후 새 매칭 시작
- **(C)** 게임 종료 시 자동 리매칭

→ Phase 5 UI 완성 후 UX 흐름에 맞게 결정 예정

---

## 7. ExRunnerGameMode와의 연결 지점

Phase 4에서 ServerTravel이 완료되면 기존 `ExRunnerGameMode`의 `WaitingForPlayers` 단계가 시작된다. 현재 ExRunnerGameMode의 멀티플레이 시작 동기화 흐름(`Match_WaitingForPlayers`, `PlayerReady` 등)은 **변경하지 않는다.**

```
ExNetwork 흐름:
  Lobby 매칭 → StartGame() → ServerTravel → [ExRunnerGameMode 인계]

ExRunnerGameMode 기존 흐름 (변경 없음):
  WaitingForPlayers → 플레이어 Ready → Countdown → Playing
```

두 시스템의 연결은 **ServerTravel 완료 시점**이 자연스러운 인계 지점이다.

---

## 8. 잠재 리스크 및 대응

| 리스크 | 대응 방안 |
|---|---|
| 클라이언트가 ServerTravel 후 EOS 주소로 재접속 실패 | `?listen` 옵션 + NetDriverDefinitions(EOS P2P) 확인. PIE에서는 로컬 루프백 사용하므로 재접속 자동 처리 |
| PIE에서 ServerTravel 후 두 번째 맵 로드 시 Crash | 기존 ExRunnerGameMode 맵 경로와 호환성 확인. 기존 단일 플레이어 테스트 맵 사용 |
| MapPath가 잘못되어 ServerTravel 실패 | `UGameplayStatics::OpenLevel` 대신 `UWorld::ServerTravel` 사용 시 경로 형식 차이 주의 (`/Game/...` 형식) |
| 매칭 Ready 상태에서 호스트 판단 불명확 | Phase 4에서는 단순 자동 호출 방식. Phase 5 UI에서 "게임 시작" 버튼으로 명시적 호출 |
| ServerTravel 후 EOS Lobby 자동 파괴 여부 | ServerTravel은 Lobby를 자동 파괴하지 않음. `DestroyLobby()`를 StartGameSession 직전에 명시적 호출 여부 결정 필요 |

---

## 9. 가이드라인 준수 체크리스트

| 가이드라인 | 상태 |
|---|---|
| 1.4 단일 책임: ServerTravel은 Strategy가, API 공개는 Subsystem이 담당 | `[x]` |
| 1.7 서버 권한: StartGameSession은 서버만 실행 | `[→]` Step 2 완료 시 |
| 1.11 디버깅: 전환 시작/완료 LogExNetwork 출력 | `[→]` Step 2~3 완료 시 |
| 3.1 보고/승인: 본 Plan이 사전 보고 | `[x]` |
| 3.3 점진적 검증: 4단계 분할 | `[x]` |

---

## 10. 승인 요청

주인님, 본 Plan 검토 후 승인해주시면 Step 1부터 순차 구현에 착수하겠습니다.

**핵심 질문 — 사전 결정 필요:**

**Q1. StartGame 자동 호출 방식**
- **(A)** OnMatchFound 완료 즉시 자동으로 StartGame 호출 (Phase 4 검증용)
- **(B)** Blueprint에서 수동으로 StartGame 호출 (Phase 5 UI 버튼 연동 대비)

→ **추천: (A)** Phase 4에서 전체 흐름 자동 검증 후, Phase 5에서 (B)로 전환

**Q2. Lobby 파괴 시점**
- **(A)** StartGameSession 직전에 DestroyLobby() 호출 (새 플레이어 참가 차단)
- **(B)** ServerTravel 이후 자동 파괴에 맡김

→ **추천: (A)** 명시적 파괴가 더 안전

수정/추가가 필요한 항목이 있으면 알려주십시오.
