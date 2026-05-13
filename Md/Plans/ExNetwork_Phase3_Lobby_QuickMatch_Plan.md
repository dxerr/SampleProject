# ExNetwork Plugin Phase 3 — Lobby Provider + Quick Match 매칭 Plan

> **목적:** EOS Lobby 기반 Quick Match 매칭 흐름을 구현한다. 플레이어가 매칭을 요청하면 빈 Lobby를 검색하여 참가하고, 없으면 새로 생성하는 Quick Match 흐름을 완성한다.
> **상태:** 설계 완료 — 구현 승인 대기
> **선행 완료:** Phase 2 — EOS Connect Device ID 로그인 성공
> **후속 Plan:** Plan 4 — Lobby → Game 전환 (ServerTravel)

---

## 1. 작업 범위 (Scope)

### 1.1 본 Phase에서 진행하는 것

- `DefaultEngine.ini` — NetDriverDefinitions 등록 (실제 P2P 연결)
- `Match/ExMatchTypes.h` — 매칭 관련 USTRUCT 정의
- `Events/ExNetEvents.h` — 매칭 이벤트 델리게이트 추가
- `Providers/IExLobbyProvider.h` — Lobby 추상화 인터페이스
- `Providers/EOS/ExEOSLobbyProvider.h/cpp` — EOS IOnlineSession 기반 Lobby 구현
- `Core/ExOnlineSubsystem.h/cpp` — Quick Match 공개 API 추가
- `Strategies/ExListenServerStrategy.h/cpp` — CreateMatch/JoinMatch 실제 구현

### 1.2 본 Phase에서 진행하지 않는 것

- Lobby → 게임 맵 전환 / ServerTravel (Plan 4)
- UI 작업 (Plan 5)
- Dedicated Server 전략 (Plan 4+)
- 친구 초대 / Invite Code 방식 (Plan 6+)

---

## 2. 핵심 설계 원칙

### 2.1 EOS Lobby를 UE 표준 OSS API로 다루는 방식

공식 `OnlineSubsystemEOS`는 EOS Lobby를 `IOnlineSession` 인터페이스로 추상화한다.
EOS SDK 직접 호출 대신 UE 표준 API를 사용한다.

```
UE 표준 API       →  EOS 내부 동작
CreateSession()   →  EOS_Lobby_CreateLobby()
FindSessions()    →  EOS_Lobby_Search()
JoinSession()     →  EOS_Lobby_JoinLobby()
DestroySession()  →  EOS_Lobby_DestroyLobby()
```

### 2.2 Lobby vs Session 구분

EOS는 Lobby와 Session 두 가지 매칭 방식을 제공한다.

```
EOS Lobby   → 대기실 개념, 멤버 관리, 실시간 속성 업데이트 가능
              → Quick Match / 그룹 매칭에 적합
              → OnlineSubsystemEOS에서 CreateSession에 bUsesPresence=true로 구분

EOS Session → 단순 세션 레지스트리
              → bUsesPresence=false
```

본 Phase에서는 **EOS Lobby 방식**을 사용한다. `FOnlineSessionSettings`에서 `bUsesPresence=true`로 설정하면 EOS Lobby로 생성된다.

### 2.3 Quick Match 흐름

```
FindQuickMatch() 호출
    ↓
FindSessions() — 현재 열린 Lobby 검색
    ↓
┌── 결과 있음 → JoinSession() → 매칭 완료
└── 결과 없음 → CreateSession() → Lobby 생성 → 다른 플레이어 대기
                    ↓ (다른 플레이어가 검색 후 참가)
                    → 매칭 완료
```


---

## 3. 폴더 구조 변경 (Phase 3 추가분)

```
Source/ExNetworkRuntime/
├── ExNetworkRuntime.Build.cs      ← 유지 (변경 없음)
│
├── Core/
│   ├── ExOnlineSubsystem.h/cpp    ← 수정: QuickMatch API 추가
│   └── ... (기존 유지)
│
├── Events/
│   └── ExNetEvents.h              ← 수정: 매칭 이벤트 델리게이트 추가
│
├── Match/                         ← 신규 폴더
│   └── ExMatchTypes.h             ← 신규: FExMatchConfig, EExMatchState 등
│
├── Providers/
│   ├── IExLobbyProvider.h         ← 신규: Lobby 추상화 인터페이스
│   └── EOS/
│       ├── ExEOSAuthProvider.h/cpp ← 유지
│       ├── ExEOSLobbyProvider.h    ← 신규
│       └── ExEOSLobbyProvider.cpp  ← 신규
│
└── Strategies/
    ├── ExListenServerStrategy.h/cpp ← 수정: CreateMatch/JoinMatch 구현
    └── ExDedicatedServerStrategy.h/cpp ← 유지 (빈 골격)
```

---

## 4. 파일별 작성 명세

### 4.1 `DefaultEngine.ini` 추가 — NetDriverDefinitions

**역할:** EOS P2P 소켓을 실제 게임 네트워킹에 사용하도록 NetDriver를 등록한다.

**추가할 설정:**
```ini
[/Script/Engine.GameEngine]
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="SocketSubsystemEOS.NetDriverEOSBase",DriverClassNameFallback="OnlineSubsystemUtils.IpNetDriver")

[/Script/OnlineSubsystemEOS.NetDriverEOSBase]
bIsUsingP2PSockets=true
```

**왜 Phase 3에서 추가하는가:**
Phase 2까지는 로그인만 검증하면 됐고, 실제 P2P 연결(ServerTravel, ClientTravel)은 Phase 3부터 필요하다. NetDriver를 너무 일찍 등록하면 기존 PIE 동작에 영향을 줄 수 있으므로 매칭 구현 시점에 추가한다.

### 4.2 `Match/ExMatchTypes.h` — 매칭 데이터 모델

**역할:** 매칭 관련 타입을 한 곳에서 정의하여 여러 클래스에서 공유한다.

**정의할 타입:**

`FExMatchConfig` (USTRUCT):
- `int32 MaxPlayers` — 최대 플레이어 수 (기본 2)
- `FString MatchMode` — 게임 모드 식별자 (예: "Runner")
- `FString MapPath` — 전환할 맵 경로 (Phase 4에서 활용)

`EExMatchState` (UENUM):
- `Idle` — 매칭 대기 없음
- `Searching` — Lobby 검색 중
- `Creating` — Lobby 생성 중
- `Waiting` — Lobby 생성 완료, 다른 플레이어 대기 중
- `Joining` — 기존 Lobby 참가 중
- `Ready` — 매칭 완료 (게임 시작 가능)

`FName ExMatchSessionName`:
- 세션 이름 상수: `"ExMatch"` — UE 세션 API 호출 시 일관된 이름 사용

### 4.3 `Events/ExNetEvents.h` 수정 — 매칭 이벤트 추가

**추가할 델리게이트:**
- `FExOnMatchFoundDelegate(bool bSuccess, const FString& ErrorMessage)` — 매칭 완료
- `FExOnMatchFoundDynDelegate` — BP용 Dynamic 버전 (UExOnlineSubsystem에서 선언)


### 4.4 `Providers/IExLobbyProvider.h` — Lobby 추상화 인터페이스

**역할:** EOS Lobby 백엔드와 무관하게 동일한 인터페이스로 Lobby를 처리한다.

**주요 메서드:**
- `CreateLobby(const FExMatchConfig& Config)` — 새 Lobby 생성 (호스트 입장)
- `FindLobbies(const FExMatchConfig& Config)` — 조건에 맞는 Lobby 검색
- `JoinLobby(const FString& SessionId)` — 기존 Lobby 참가
- `DestroyLobby()` — Lobby 파괴 및 정리
- `IsInLobby() const` — 현재 Lobby 참가 여부
- 델리게이트: `OnCreateComplete`, `OnFindComplete`, `OnJoinComplete`, `OnDestroyComplete`

**설계 원칙:**
- 순수 인터페이스 (UObject 기반 아님)
- `UExOnlineSubsystem`이 TUniquePtr로 소유
- 구현체는 `Initialize(IOnlineSubsystem* OSS)`를 통해 OSS 참조 주입

### 4.5 `Providers/EOS/ExEOSLobbyProvider.h/cpp` — EOS Lobby 구현

**역할:** `IExLobbyProvider`의 EOS 구현체. `IOnlineSession` 인터페이스를 통해 EOS Lobby를 생성/검색/참가한다.

**CreateLobby 동작:**
1. `IOnlineSession::CreateSession()` 호출
2. `FOnlineSessionSettings` 구성:
   - `bUsesPresence = true` → EOS Lobby 경로 사용
   - `NumPublicConnections = Config.MaxPlayers`
   - `bShouldAdvertise = true` → 검색 가능하도록 공개
   - `bAllowJoinInProgress = false` → 게임 시작 후 참가 차단
   - Custom 속성: `"MatchMode"` = `Config.MatchMode`
3. `OnCreateSessionComplete` 콜백에서 결과 전달

**FindLobbies 동작:**
1. `IOnlineSession::FindSessions()` 호출
2. `FOnlineSessionSearch` 구성:
   - `MaxSearchResults = 10`
   - `bIsLanQuery = false`
   - Custom 필터: `"MatchMode"` = `Config.MatchMode`
3. `OnFindSessionsComplete` 콜백에서 결과 반환

**JoinLobby 동작:**
1. `FindLobbies` 결과의 `FOnlineSessionSearchResult` 활용
2. `IOnlineSession::JoinSession()` 호출
3. `OnJoinSessionComplete` 콜백에서 결과 전달

**Quick Match 자동 흐름 (ExListenServerStrategy에서 호출):**
```
FindLobbies()
  → 결과 있음: JoinLobby(첫 번째 결과)
  → 결과 없음: CreateLobby()
```

### 4.6 `Strategies/ExListenServerStrategy.h/cpp` 수정

**Phase 3에서 채워지는 내용:**
- `CreateMatch(const FExMatchConfig& Config)` → `LobbyProvider->CreateLobby(Config)` 호출
- `JoinMatch(const FString& SessionId)` → `LobbyProvider->JoinLobby(SessionId)` 호출

**Quick Match 구현:**
- `FindAndJoinOrCreate(const FExMatchConfig& Config)` — 검색 후 참가 또는 생성 자동 처리
- `LobbyProvider->FindLobbies()` 결과에 따라 자동 분기

**LobbyProvider 소유:**
- `ExListenServerStrategy`가 `TUniquePtr<IExLobbyProvider> LobbyProvider` 보유
- `UExOnlineSubsystem::Initialize` 시 `ExEOSLobbyProvider` 주입

### 4.7 `Core/ExOnlineSubsystem.h/cpp` 수정

**Phase 3에서 추가되는 공개 API:**

```cpp
// Quick Match 시작 — Lobby 검색 후 참가 또는 생성
UFUNCTION(BlueprintCallable, Category = "ExNetwork|Match")
void FindQuickMatch(const FExMatchConfig& Config);

// 매칭 취소
UFUNCTION(BlueprintCallable, Category = "ExNetwork|Match")
void CancelMatch();

// 현재 매칭 상태 반환
UFUNCTION(BlueprintPure, Category = "ExNetwork|Match")
EExMatchState GetMatchState() const;

// 매칭 완료 델리게이트
UPROPERTY(BlueprintAssignable, Category = "ExNetwork|Match")
FExOnMatchFoundDynDelegate OnMatchFound;
```

**내부 상태 관리:**
- `EExMatchState CurrentMatchState` — 현재 매칭 단계 추적
- 로그인 확인 후 매칭 시작 (`IsLoggedIn()` 체크)
- 로그인 미완료 시 경고 로그 후 리턴


---

## 5. 구현 단계 (Implementation Steps)

### Step 1 — NetDriverDefinitions 등록 + PIE 회귀 검증

**작업:**
- `DefaultEngine.ini`에 NetDriverDefinitions 추가
- 빌드 후 기존 PIE 정상 동작 확인

**검증:**
```
LogNet: GameNetDriver EOS started
기존 Runner 게임 PIE 3-Client 정상 동작
```

### Step 2 — Match 폴더 + 타입 정의

**작업:**
- `Source/ExNetworkRuntime/Match/` 폴더 생성
- `ExMatchTypes.h` 작성
- `Build.cs`에 `Match/` 폴더 PublicIncludePaths 추가

**검증:** 빌드 통과

### Step 3 — IExLobbyProvider 인터페이스 + Events 수정

**작업:**
- `Providers/IExLobbyProvider.h` 작성
- `Events/ExNetEvents.h`에 매칭 델리게이트 추가

**검증:** 빌드 통과

### Step 4 — ExEOSLobbyProvider 구현

**작업:**
- `Providers/EOS/ExEOSLobbyProvider.h/cpp` 작성
- CreateLobby / FindLobbies / JoinLobby / DestroyLobby 구현

**검증:** 빌드 통과

### Step 5 — ExListenServerStrategy Quick Match 구현

**작업:**
- `Strategies/ExListenServerStrategy.h/cpp` CreateMatch/JoinMatch 실제 구현
- Quick Match 자동 흐름 (검색 → 참가 또는 생성) 구현

**검증:** 빌드 통과

### Step 6 — ExOnlineSubsystem QuickMatch API 통합 + 전체 흐름 검증

**작업:**
- `Core/ExOnlineSubsystem.h/cpp` FindQuickMatch / CancelMatch / GetMatchState 추가
- 매칭 상태 머신 연결

**검증:**
```
[ExEOSLobbyProvider] CreateLobby 시작 — MatchMode=Runner, MaxPlayers=2
[ExEOSLobbyProvider] Lobby 생성 완료 — SessionId=...
[UExOnlineSubsystem] 매칭 상태: Waiting → 다른 플레이어 대기 중

(다른 PIE 인스턴스에서 FindQuickMatch 호출 시)
[ExEOSLobbyProvider] FindLobbies 결과 — 1개 발견
[ExEOSLobbyProvider] JoinLobby 시작 — SessionId=...
[ExEOSLobbyProvider] JoinLobby 완료 — 성공
[UExOnlineSubsystem] OnMatchFound 브로드캐스트 — 성공
```

---

## 6. NetDriverDefinitions 상세 설명

### 6.1 왜 필요한가

`DefaultPlatformService=EOS`로 설정해도 **네트워크 패킷 전송 자체는 여전히 기본 IpNetDriver(UDP)**를 사용한다. EOS P2P를 실제 게임 통신에 사용하려면 NetDriver를 EOS 기반으로 교체해야 한다.

```
NetDriverDefinitions 미설정:
  ServerTravel → 클라이언트가 호스트 IP로 직접 UDP 연결 시도
  → 방화벽, NAT으로 인해 인터넷에서 실패 가능

NetDriverDefinitions 설정:
  ServerTravel → EOS P2P 릴레이를 통해 연결
  → NAT 통과, 방화벽 우회 가능
  → 인터넷 매칭 정상 동작
```

### 6.2 Fallback 정책

```
DriverClassName="SocketSubsystemEOS.NetDriverEOSBase"
  → EOS P2P 소켓 사용 (NAT 통과)

DriverClassNameFallback="OnlineSubsystemUtils.IpNetDriver"
  → EOS 소켓 실패 시 표준 UDP로 폴백
  → LAN 환경에서도 동작 보장
```

---

## 7. 잠재 리스크 및 대응

| 리스크 | 대응 방안 |
|---|---|
| NetDriverDefinitions 추가 후 PIE 회귀 | Step 1에서 즉시 PIE 검증. 문제 시 롤백 |
| EOS Lobby 검색 결과가 항상 0개 반환 | `bShouldAdvertise=true`, `bUsesPresence=true` 설정 확인. EOS Portal 클라이언트 정책 Lobbies 활성화 확인 |
| PIE에서 같은 PC의 여러 인스턴스가 서로 찾지 못하는 문제 | PIE는 같은 EOS Product 내에서 동작하므로 정상 검색 가능. 단, 포트 충돌 가능성 — `bIsUsingP2PSockets=true` 확인 |
| JoinLobby 후 실제 P2P 연결 실패 | Phase 3는 Lobby 참가까지만 검증. 실제 ServerTravel은 Phase 4에서 처리 |
| `FExMatchConfig`가 Blueprint에 노출 안 되는 문제 | USTRUCT에 BlueprintType 추가 |

---

## 8. 변경하지 않는 것 (Out of Scope)

- ServerTravel / ClientTravel (Plan 4)
- 기존 ExRunnerPlay 멀티플레이 흐름
- UI 위젯 (Plan 5)
- ExDedicatedServerStrategy 구현 (Plan 4+)
- 친구 초대 / Invite Code (Plan 6+)

---

## 9. 가이드라인 준수 체크리스트

| 가이드라인 | 상태 |
|---|---|
| 1.2 명명 규칙 | `[→]` Step 2 완료 시 |
| 1.4 단일 책임: LobbyProvider가 Lobby만 담당 | `[x]` 설계 단계 보장 |
| 1.5 폴더 구조: Match/ 신규 폴더 추가 | `[→]` Step 2 완료 시 |
| 1.7 검증: 실패 경로 모두 로그 출력 | `[→]` Step 4 완료 시 |
| 1.11 디버깅: LogExNetwork 카테고리 일관 사용 | `[→]` Step 4~6 완료 시 |
| 3.1 보고/승인: 본 Plan이 사전 보고 | `[x]` |
| 3.3 점진적 검증: 6단계 분할 | `[x]` |

---

## 10. 승인 요청

주인님, 본 Plan 검토 후 승인해주시면 Step 1부터 순차 구현에 착수하겠습니다.

수정/추가가 필요한 항목이 있으면 알려주십시오.
