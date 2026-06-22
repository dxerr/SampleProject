# ExNetwork Plugin Phase 2 — 인증 + 인터페이스 코어 구축 Plan

> **목적:** EOS Device ID 기반 자동 로그인을 구현하고, 서버 모델 추상화(Strategy Pattern) 및 공통 이벤트 델리게이트를 구축하는 단계.
> **상태:** 설계 완료 — 구현 승인 대기
> **선행 완료:** Phase 1 — 플러그인 골격 + EOS SDK 통신 확인
> **후속 Plan:** Plan 3 — Lobby Provider + Quick Match 매칭

---

## 1. 작업 범위 (Scope)

### 1.1 본 Phase에서 진행하는 것

- `DefaultEngine.ini` — `DefaultPlatformService=EOS` 전환 + EOS Connect/EAS 설정
- `ExNetworkRuntime.Build.cs` — `OnlineSubsystem`, `OnlineSubsystemUtils` 의존성 추가
- `Core/IExAuthProvider.h` — 인증 추상화 인터페이스
- `Core/IExNetServerStrategy.h` — 서버 모델 추상화 인터페이스
- `Events/ExNetEvents.h` — 공통 델리게이트 정의
- `Providers/EOS/ExEOSAuthProvider.h/cpp` — Device ID 자동 로그인 구현
- `Providers/Null/ExNullAuthProvider.h/cpp` — PIE/오프라인 테스트용 인증 구현
- `Strategies/ExListenServerStrategy.h/cpp` — Listen Server 동작 구현
- `Strategies/ExDedicatedServerStrategy.h/cpp` — 빈 골격 (Phase 4 이후 구현)
- `Core/ExOnlineSubsystem.h/cpp` — 부팅 시 자동 인증 흐름 추가

### 1.2 본 Phase에서 진행하지 않는 것

- Lobby 생성/검색/참가 (Plan 3)
- Matchmaking (Plan 3+)
- Lobby → Game 전환 (Plan 4)
- NetDriverDefinitions 등록 (Plan 3 — 실제 P2P 매치 시점)
- UI 작업 (Plan 5)

---

## 2. 인증 흐름 설계 (Auth Flow Design)

### 2.1 공식 OnlineSubsystemEOS의 인증 구조 이해

공식 플러그인은 두 가지 인증 레이어를 제공한다:

```
EAS (Epic Account Services)  — Epic 계정 로그인 (bUseEAS=true)
EOS Connect                  — 익명/Device ID 로그인 (bUseEOSConnect=true)
```

본 프로젝트는 **"회원가입 절차 없는 익명 로그인"** 이 목표이므로 `EOS Connect + Device ID` 방식을 사용한다. EAS는 사용하지 않는다.

### 2.2 Device ID 로그인 흐름

```
UExOnlineSubsystem::Initialize()
    ↓
ExEOSAuthProvider::Login()
    ↓
IOnlineIdentity::AutoLogin(0)      ← UE OnlineSubsystem 표준 API 호출
    ↓ (내부 흐름 — EOS 공식 플러그인이 처리)
EOS_Connect_CreateDeviceId()       ← 디바이스 고유 ID 생성 (최초 1회)
    ↓
EOS_Connect_Login()                ← Device ID로 EOS Connect 로그인
    ↓
OnLoginCompleteDelegate 브로드캐스트
    ↓
ExEOSAuthProvider::OnLoginComplete() 콜백
    ↓
UExOnlineSubsystem::OnAuthComplete() → 상위에 결과 전달
```

### 2.3 PIE 멀티 클라이언트 처리

PIE 실행 시 GameInstance가 3개(Server + 2 Client) 생성되며, 각각 독립적으로 `AutoLogin`이 호출된다. EOS Connect Device ID는 인스턴스별로 고유 ID를 생성하므로 PIE에서도 정상 동작한다.

---


## 3. 폴더 구조 변경 (Phase 2 추가분)

```
Source/ExNetworkRuntime/
├── ExNetworkRuntime.Build.cs      ← 수정: OnlineSubsystem 의존성 추가
├── ExNetworkRuntimeModule.h/cpp   ← 유지
│
├── Core/                          ← 수정/추가
│   ├── ExOnlineSubsystem.h/cpp    ← 수정: 인증 흐름 추가
│   ├── ExNetworkLog.h             ← 유지
│   ├── IExAuthProvider.h          ← 신규: 인증 인터페이스
│   └── IExNetServerStrategy.h     ← 신규: 서버 모델 인터페이스
│
├── Events/                        ← 신규 폴더
│   └── ExNetEvents.h              ← 신규: 공통 델리게이트
│
├── Providers/                     ← 신규 폴더
│   ├── EOS/                       ← 신규 폴더
│   │   ├── ExEOSAuthProvider.h    ← 신규
│   │   └── ExEOSAuthProvider.cpp  ← 신규
│   └── Null/                      ← 신규 폴더
│       ├── ExNullAuthProvider.h   ← 신규
│       └── ExNullAuthProvider.cpp ← 신규
│
└── Strategies/                    ← 신규 폴더
    ├── ExListenServerStrategy.h   ← 신규
    ├── ExListenServerStrategy.cpp ← 신규
    ├── ExDedicatedServerStrategy.h   ← 신규 (빈 골격)
    └── ExDedicatedServerStrategy.cpp ← 신규 (빈 골격)
```

---

## 4. 파일별 작성 명세

### 4.1 `ExNetworkRuntime.Build.cs` 수정

**추가할 의존성:**
- `PublicDependencyModuleNames`: `OnlineSubsystem`, `OnlineSubsystemUtils` 추가
- `PublicIncludePaths`: `Events/`, `Providers/`, `Providers/EOS/`, `Providers/Null/`, `Strategies/` 추가

### 4.2 `Core/IExAuthProvider.h` — 인증 추상화 인터페이스

**역할:** 인증 백엔드(EOS/Steam/Custom)와 무관하게 동일한 인터페이스로 인증을 처리한다.

**주요 개념:**
- `Login(int32 LocalUserNum)` — 비동기 로그인 시작
- `Logout(int32 LocalUserNum)` — 로그아웃
- `IsLoggedIn(int32 LocalUserNum) const` — 로그인 상태 확인
- `OnLoginComplete` 델리게이트 — 로그인 결과 통보

**설계 원칙:**
- 순수 인터페이스 클래스 (UObject 기반 아님 — 생명주기 관리 단순화)
- 구현체는 `UExOnlineSubsystem`이 소유하며 TUniquePtr 또는 SharedPtr로 관리
- 구현체는 `Initialize(IOnlineSubsystem* InOSS)`를 통해 OSS 참조를 주입받음


### 4.3 `Core/IExNetServerStrategy.h` — 서버 모델 추상화 인터페이스

**역할:** Listen Server / Dedicated Server 두 모델을 동일한 인터페이스로 추상화한다.

**주요 개념:**
- `CreateMatch(const FExMatchConfig& Config)` — 매치 생성 (호스트 입장)
- `JoinMatch(const FExMatchHandle& Handle)` — 매치 참가 (클라이언트 입장)
- `StartGameSession()` — Lobby에서 게임 맵으로 전환 트리거
- `DestroyMatch()` — 매치 정리
- `GetServerType() const` — 현재 전략 타입 반환 (`EEx_Listen` / `EEx_Dedicated`)

**Phase 2 구현 범위:**
- `ExListenServerStrategy` — `CreateMatch`, `JoinMatch` 빈 골격 (Phase 3에서 채워짐)
- `ExDedicatedServerStrategy` — 전체 빈 골격 (Phase 4+)

**환경 자동 감지 정책 (UExOnlineSubsystem에서 사용):**
```
GetNetMode() == NM_DedicatedServer → ExDedicatedServerStrategy
그 외 → ExListenServerStrategy
```

### 4.4 `Events/ExNetEvents.h` — 공통 델리게이트 정의

**역할:** ExNetwork 전체에서 사용되는 이벤트 델리게이트를 한 곳에서 정의한다.

**정의할 델리게이트:**
- `FExOnLoginComplete(bool bSuccess, const FString& ErrorMessage)` — 로그인 결과
- `FExOnLogoutComplete(bool bSuccess)` — 로그아웃 결과
- `FExOnMatchFound(const FExMatchHandle& Handle)` — 매치 발견 (Phase 3)
- `FExOnMatchJoined(bool bSuccess)` — 매치 참가 결과 (Phase 3)

**설계 원칙:**
- DECLARE_MULTICAST_DELEGATE 계열 사용 (Blueprint 노출이 필요하면 DECLARE_DYNAMIC_MULTICAST_DELEGATE)
- Phase 2에서는 Login/Logout 두 개만 정의, 나머지는 Phase 3 시점에 추가
- 가이드라인 1.6 Doxygen 주석 작성

### 4.5 `Providers/EOS/ExEOSAuthProvider.h/cpp` — EOS Device ID 인증 구현

**역할:** `IExAuthProvider`의 EOS 구현체. `IOnlineIdentity::AutoLogin`을 통해 Device ID 로그인을 수행한다.

**Login 동작 흐름:**
1. `IOnlineSubsystem::Get("EOS")` 로 EOS OSS 참조 획득
2. `IOnlineIdentity` 인터페이스 획득
3. `AddOnLoginCompleteDelegate_Handle` 으로 콜백 등록
4. `Identity->AutoLogin(LocalUserNum)` 호출
5. 콜백에서 성공/실패 결과를 `OnLoginComplete` 델리게이트로 전파
6. `ensureMsgf` 로 실패 케이스 디버깅 가능하게 처리

**Initialize 의도:**
- `bUseEOSConnect=true` 설정이 ini에 있어야 Device ID 로그인이 동작함
- 생성자에서 OSS 참조를 캐싱하되 `nullptr` 시 graceful degradation

### 4.6 `Providers/Null/ExNullAuthProvider.h/cpp` — 테스트용 인증 구현

**역할:** PIE 오프라인 테스트 또는 EOS 서버 미연결 환경에서 즉시 로그인 성공을 시뮬레이션한다.

**동작:**
- `Login()` 호출 시 `GetWorld()->GetTimerManager()` 를 통해 다음 틱에 `OnLoginComplete(true, "")` 발동
- `IsLoggedIn()` 은 항상 true 반환
- `UE_LOG(LogExNetwork, Warning, "NullAuthProvider: Simulating login success")` 출력

**언제 사용되는가:**
- `UExOnlineSubsystem::Initialize`에서 OSS 가져오기 실패 시 Null Provider로 fallback
- 개발 중 EOS 연결 없이 매칭 흐름을 테스트할 때 수동으로 활성화 가능


### 4.7 `Strategies/ExListenServerStrategy.h/cpp`

**역할:** Listen Server 환경에서의 매치 생성/참가 전략 구현.

**Phase 2 범위:**
- 클래스 선언 및 `IExNetServerStrategy` 상속
- `GetServerType()` → `EExServerType::ListenServer` 반환
- `CreateMatch`, `JoinMatch`, `StartGameSession`, `DestroyMatch` 빈 구현 (로그만 출력)
- Phase 3에서 EOS Lobby 생성/검색 로직으로 채워짐

### 4.8 `Strategies/ExDedicatedServerStrategy.h/cpp`

**역할:** Dedicated Server 환경 전략. Phase 2에서는 전체 빈 골격.

**Phase 2 범위:**
- 클래스 선언 및 `IExNetServerStrategy` 상속
- `GetServerType()` → `EExServerType::DedicatedServer` 반환
- 모든 함수 초입에 `UE_LOG(LogExNetwork, Warning, "DedicatedServerStrategy: Not implemented yet")` 출력
- Phase 4+ 에서 Dedicated Server 론칭 로직으로 채워짐

### 4.9 `Core/ExOnlineSubsystem.h/cpp` 수정

**Phase 2에서 추가되는 내용:**

**멤버 변수:**
- `TUniquePtr<IExAuthProvider> AuthProvider` — 현재 인증 Provider
- `TUniquePtr<IExNetServerStrategy> ServerStrategy` — 현재 서버 Strategy
- `bool bIsLoggedIn = false` — 로그인 상태 캐시

**`Initialize()` 변경:**
1. `IOnlineSubsystem::Get()` 으로 OSS 획득
2. OSS 유효하면 `ExEOSAuthProvider` 생성, 없으면 `ExNullAuthProvider` fallback
3. NetMode 확인하여 `ExListenServerStrategy` 또는 `ExDedicatedServerStrategy` 선택
4. `AuthProvider->Login(0)` 호출 → 자동 로그인 시작
5. `OnLoginComplete` 델리게이트 바인딩

**신규 공개 API (Phase 2):**
- `bool IsLoggedIn() const` (BlueprintPure)
- `FExOnLoginComplete OnLoginComplete` (BlueprintAssignable)

---

## 5. DefaultEngine.ini 변경 (Phase 2)

### 5.1 추가할 설정

기존 ExNetwork EOS 블록 아래에 다음을 추가한다:

**OSS 기본값 전환 (Phase 2 핵심):**
```ini
[OnlineSubsystem]
DefaultPlatformService=EOS
```

**EOS Connect 활성화 (Device ID 로그인에 필수):**
```ini
[/Script/OnlineSubsystemEOS.EOSSettings]
bUseEOSConnect=true
bUseEAS=false
```

**모바일 Android 빌드 대비 (비민감 설정):**
```ini
[OnlineSubsystemEOS]
bEnabled=true
```

### 5.2 Phase 2 이전 상태와의 비교

| 항목 | Phase 1 | Phase 2 |
|---|---|---|
| DefaultPlatformService | NULL (기본값) | **EOS** |
| bUseEOSConnect | 미설정 | **true** |
| bUseEAS | 미설정 | **false** |
| 인증 동작 | 없음 | **자동 로그인** |

### 5.3 주의사항

`DefaultPlatformService=EOS` 전환 후 기존 PIE 동작 회귀 여부를 즉시 검증해야 한다. 특히 ExRunnerPlay의 기존 멀티플레이 흐름(Match_WaitingForPlayers 등)이 영향을 받지 않아야 한다.


## 6. 구현 단계 (Implementation Steps)

### Step 1 — DefaultEngine.ini 변경 + 빌드 검증

**작업:**
- `DefaultPlatformService=EOS` 추가
- `bUseEOSConnect=true`, `bUseEAS=false` 추가
- 빌드 통과 확인
- 기존 PIE 회귀 없음 확인

**검증:**
- `LogOnline: OnlineSubsystemEOS initialized successfully` 출력 확인
- 기존 Runner 게임 PIE 정상 동작 확인

### Step 2 — Build.cs 의존성 추가 + 폴더 생성

**작업:**
- `ExNetworkRuntime.Build.cs`에 `OnlineSubsystem`, `OnlineSubsystemUtils` 추가
- `PublicIncludePaths`에 신규 폴더 등록
- 신규 폴더 생성: `Events/`, `Providers/EOS/`, `Providers/Null/`, `Strategies/`

**검증:** 빌드 통과

### Step 3 — 인터페이스 + 이벤트 헤더 작성

**작업:**
- `Core/IExAuthProvider.h`
- `Core/IExNetServerStrategy.h`
- `Events/ExNetEvents.h`

**검증:** 빌드 통과 (헤더 전용, 구현 없음)

### Step 4 — Provider 구현

**작업:**
- `Providers/EOS/ExEOSAuthProvider.h/cpp`
- `Providers/Null/ExNullAuthProvider.h/cpp`

**검증:** 빌드 통과

### Step 5 — Strategy 구현

**작업:**
- `Strategies/ExListenServerStrategy.h/cpp`
- `Strategies/ExDedicatedServerStrategy.h/cpp`

**검증:** 빌드 통과

### Step 6 — ExOnlineSubsystem 통합 + 자동 로그인 흐름

**작업:**
- `Core/ExOnlineSubsystem.h/cpp` Phase 2 변경 사항 적용
- 부팅 시 자동 로그인 흐름 완성

**검증:**
```
LogExNetwork: [UExOnlineSubsystem] AuthProvider 선택: EOS
LogExNetwork: [UExOnlineSubsystem] ServerStrategy 선택: ListenServer
LogExNetwork: [ExEOSAuthProvider] Login 시작 — LocalUserNum=0
LogExNetwork: [ExEOSAuthProvider] Login 완료 — 성공
```

---

## 7. 잠재 리스크 및 대응

| 리스크 | 대응 방안 |
|---|---|
| `DefaultPlatformService=EOS` 전환 후 기존 PIE 회귀 | Step 1에서 즉시 기존 Runner 게임 PIE 검증. 문제 시 즉시 설정 롤백 |
| PIE에서 EOS Connect Device ID 충돌 (같은 PC에서 여러 인스턴스) | EOS Connect는 PIE 인스턴스별 고유 ID 생성. 실제 충돌 시 NullAuthProvider fallback |
| `IOnlineSubsystem::Get()` 이 nullptr 반환 (OSS 초기화 지연) | `UExOnlineSubsystem::Initialize`에서 `nullptr` 시 NullAuthProvider로 graceful fallback. `ensureMsgf` 로깅 |
| `bUseEOSConnect=true` 설정 누락 시 Device ID 로그인 실패 | Step 1 검증 단계에서 `LogOnline` 필터로 "Neither EAS or EOS" 에러 감지 |
| 기존 ExRunnerGameMode의 멀티플레이 흐름과 OSS 간섭 | ExRunnerPlay는 OnlineSubsystem을 직접 사용하지 않으므로 간섭 없음 |

---

## 8. 변경하지 않는 것 (Out of Scope)

- EOS Lobby 생성/검색/참가 (Plan 3)
- NetDriverDefinitions 등록 (Plan 3)
- Lobby → Game 전환 (Plan 4)
- ExRunnerPlay의 기존 멀티플레이 시작 동기화 흐름
- UI 위젯 (Plan 5)
- ExDedicatedServerStrategy 실제 구현 (Phase 4+)

---

## 9. 가이드라인 준수 체크리스트

| 가이드라인 | 상태 |
|---|---|
| 1.2 명명 규칙: IEx 접두사 인터페이스 | `[→]` Step 3 완료 시 |
| 1.4 단일 책임: Provider/Strategy 각 파일이 단일 책임 | `[→]` Step 4~5 완료 시 |
| 1.5 폴더 구조: ExFrameWork 정책 (Public/Private 없음) | `[→]` Step 2 완료 시 |
| 1.7 검증/체크: 실패 경로 ensure + NullProvider fallback | `[→]` Step 4 완료 시 |
| 1.11 디버깅: 모든 주요 흐름에 LogExNetwork 출력 | `[→]` Step 6 완료 시 |
| 4.1 의존성 방향: ExNetwork → OnlineSubsystem (단방향) | `[x]` 설계 단계에서 보장 |

---

## 10. 승인 요청

주인님, 본 Plan 검토 후 승인해주시면 Step 1부터 순차 구현에 착수하겠습니다.

수정/추가가 필요한 항목이 있으면 알려주십시오.
