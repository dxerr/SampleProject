# ExNetwork Plugin Phase 1 — 플러그인 골격 + EOS SDK 통합 Plan (v2)

> **목적:** 매칭/온라인 시스템을 위한 신규 일반 플러그인 `ExNetwork`의 골격을 구축하고, Redpoint EOS Online Framework를 통합하여 후속 Phase에서 EOS를 활용할 수 있는 토대를 마련하는 첫 단계.
> **상태:** 외부 AI 피드백 + ExFrameWork 정책 반영 v2 — 구현 승인 대기
> **작업 분류:** 신규 플러그인 생성(부분 기완료), 외부 플러그인 도입, 설정 파일 최소 변경
> **선행 작업 필요:** 주인님의 EOS Developer Portal 가입은 **Phase 2 진입 전까지** 완료 필요. Phase 1 자체는 EOS 정보 없이도 진행 가능.
> **후속 Plan:** Plan 2 — 인증 + 인터페이스 코어 구축 (DefaultPlatformService 전환, NetDriver 등록 포함)

---

## 변경 이력 (Change Log)

### v2 (외부 AI 피드백 + ExFrameWork 정책 반영)
- **[정책 통일]** Public/Private 폴더 분리 제거 → ExFrameWork 코드베이스 정책에 맞춰 `.h`와 `.cpp`를 같은 위치에 배치하는 평면 구조로 변경
- **[명칭 통일]** 모든 `.Build.cs` 및 모듈 표기를 `ExNetworkRuntime`으로 일원화 (ExCore의 `ExCoreRuntime` 패턴 일치)
- **[범위 축소]** Phase 1에서 `DefaultPlatformService=EOS` 전환을 제거 → Phase 2(인증 진입)로 이동. 회귀 리스크 차단
- **[범위 명시]** EOS Product 정보 입력도 Phase 2로 이동. Phase 1은 EOS 가입 없이도 완료 가능
- **[누락 보강]** NetDriverDefinitions 등록 항목을 명시적으로 Phase 3 작업으로 예고
- **[현실 반영]** Step 1을 "신규 생성"에서 "기존 폴더 골격 보강"으로 정정 (이미 `Plugins/ExNetwork/Md/`는 존재)
- **[의존성 최소화]** Slate/SlateCore/DeveloperSettings 등 UI 의존성 제거 → Phase 1은 Core/CoreUObject/Engine만 의존
- **[보안 강화]** 시크릿 관리 방식 확정을 Phase 2 진입 전 필수 게이트로 명시
- **[체크리스트 정정]** 가이드라인 준수 항목을 `[x] 충족 / [→] Phase 1 완료 시 / [ ] 후속 Phase`로 3종 분리

### v1 (초안)
- 신규 플러그인 골격 + EOS SDK 통합 초안 작성


---

## 1. 작업 범위 (Scope)

### 1.1 본 Phase에서 진행하는 것

- 기존 `Plugins/ExNetwork/` 폴더에 플러그인 골격 추가 (Md/ 폴더는 이미 존재)
- `ExNetwork.uplugin` 매니페스트 작성
- `Source/ExNetworkRuntime/` 모듈 폴더 및 빌드 구성 작성
- `ExNetworkRuntime.Build.cs` 최소 의존성 정의
- `ExNetworkRuntimeModule.h/cpp` 모듈 진입점 작성
- `Core/ExOnlineSubsystem.h/cpp` 빈 골격 (GameInstanceSubsystem)
- `Core/ExNetworkLog.h` 로그 카테고리 선언
- Redpoint EOS Online Framework 플러그인 설치 및 활성화 (단, OSS 기본값 전환은 미수행)
- 빌드 통과 검증 (Windows Editor 기준)
- 기존 아키텍처 문서 갱신 (Public/Private 표기 제거, 폴더 구조 갱신)

### 1.2 본 Phase에서 진행하지 않는 것

- `DefaultPlatformService=EOS` 전환 (Phase 2로 이동)
- EOS Product 정보 입력 (Phase 2로 이동)
- NetDriverDefinitions 등록 (Phase 3로 이동)
- 인증 로직 구현 (Plan 2)
- 서버 Strategy 구현 (Plan 2)
- Lobby/Matchmaking 로직 (Plan 3)
- Lobby → Game 전환 로직 (Plan 4)
- UI 작업 (Plan 5)
- 매칭 방식 확장 (Plan 6+)
- 안드로이드 빌드 검증 (별도 Phase)

### 1.3 Phase 1의 정의 (한 줄 요약)

> **"빌드 가능한 빈 플러그인 + Redpoint 플러그인 로드 가능한 상태"** 까지가 Phase 1의 완료 정의.

이 정의는 Phase 1 검수 시 명확한 기준점이 된다.


---

## 2. 사전 준비 사항 (Prerequisites)

### 2.1 Phase 1 진입 전 필수 사항

- **Redpoint EOS Online Framework 입수** — Fab 마켓플레이스에서 Free 또는 Paid Edition 다운로드
- Free Edition 사용 시 EOS SDK 별도 다운로드 (Phase 1 빌드 통과를 위해 Redpoint 플러그인이 정상 로드되어야 함)

### 2.2 Phase 2 진입 전 필수 사항 (Phase 1 동안 병행 진행)

다음은 Phase 1 구현과 병행하여 진행하고, Phase 2 진입 전까지 확보되어야 한다.

1. **EOS Developer Portal 가입**
   - https://dev.epicgames.com/portal/ 접속
   - Organization 생성

2. **Product 등록 및 발급 정보 확보**
   - Product ID
   - Sandbox ID
   - Deployment ID
   - Client ID (Game Client용)
   - Client Secret (Game Client용)
   - Encryption Key

3. **시크릿 관리 방식 결정 (v2 필수 게이트)**
   - 권장 방식:
     1. `DefaultEngine.ini`에는 placeholder만 (예: `ClientSecret="<SET_LOCALLY>"`)
     2. 실제 값은 `Saved/Config/Windows/Engine.ini` 또는 환경변수
     3. `.gitignore`에 시크릿 ini 추가
   - Phase 1 진행 중 주인님과 협의하여 확정
   - 미확정 상태로 Phase 2 진입 금지

### 2.3 Phase 1 자체는 EOS 정보 없이도 완료 가능

Phase 1은 빌드 골격 단계이므로 Product 정보가 없어도 검증 가능하다. Redpoint 플러그인이 로드되고, `UExOnlineSubsystem`이 정상 Initialize 되는 것까지가 검증 기준이다.


---

## 3. 플러그인 디렉토리 구조 (Plugin Layout)

### 3.1 ExFrameWork 정책 — Public/Private 분리 미사용

본 플러그인은 ExFrameWork의 표준 정책을 따른다. ExCore를 비롯한 기존 모듈은 `.h`와 `.cpp` 파일을 **같은 위치에 평면 배치**하며, `Public/`, `Private/` 폴더 분리를 사용하지 않는다. 외부 모듈의 헤더 참조는 `Build.cs`의 `PublicIncludePaths`에 서브폴더를 명시적으로 등록하여 해결한다.

본 플러그인도 이 정책을 따른다.

### 3.2 Phase 1 완료 시점의 최종 구조

```
Plugins/
└── ExNetwork/                                    ← 기존 폴더 (Md/ 이미 존재)
    ├── ExNetwork.uplugin                          ← Phase 1: 신규 작성
    ├── Resources/
    │   └── Icon128.png                            ← Phase 1: 임시 아이콘
    ├── Config/
    │   └── FilterPlugin.ini                       ← Phase 1: 빈 파일
    ├── Md/                                        ← 기존 (v2에서 갱신)
    │   ├── ExNetwork_Architecture_Summary.md
    │   └── ExNetwork_Module_Layout.md
    └── Source/
        └── ExNetworkRuntime/                      ← Phase 1: 신규 모듈
            ├── ExNetworkRuntime.Build.cs           ← 신규
            ├── ExNetworkRuntimeModule.h            ← 신규
            ├── ExNetworkRuntimeModule.cpp          ← 신규
            └── Core/                               ← Phase 1에서 채워지는 유일한 서브폴더
                ├── ExOnlineSubsystem.h             ← 신규 (빈 골격)
                ├── ExOnlineSubsystem.cpp           ← 신규 (빈 골격)
                └── ExNetworkLog.h                  ← 신규
```

### 3.3 명명 규칙 정합성 (가이드라인 1.2)

- 플러그인명: `ExNetwork`
- 모듈명: `ExNetworkRuntime` (ExCore의 `ExCoreRuntime` 패턴 일치)
- 클래스 접두사: `UEx` / `AEx` / `FEx` / `IEx`
- 폴더 구조: 가이드라인 1.5에 따라 책임별 서브폴더 (`Core/`, 향후 `Strategies/`, `Providers/` 등)

### 3.4 Phase 1에서 생성하지 않는 폴더

다음 폴더들은 후속 Plan에서 생성된다:

- `Source/ExNetworkRuntime/Strategies/` — Plan 2
- `Source/ExNetworkRuntime/Providers/EOS/` — Plan 2~3
- `Source/ExNetworkRuntime/Providers/Null/` — Plan 2
- `Source/ExNetworkRuntime/Match/` — Plan 3
- `Source/ExNetworkRuntime/Player/` — Plan 3
- `Source/ExNetworkRuntime/Events/` — Plan 2
- `Source/ExNetworkEditor/` — Phase 5+ 검토


---

## 4. 파일별 작성 명세

### 4.1 `ExNetwork.uplugin`

**역할:** 플러그인 메타데이터 정의 파일.

**핵심 필드 의도:**
- `FriendlyName`: "Ex Network"
- `Description`: 매칭/온라인 통합 모듈
- `Category`: "Networking"
- `EnabledByDefault`: true
- `CanContainContent`: false (Phase 1 기준, BP 자원 추가 시점에 true로 변경)
- `IsBetaVersion`: false
- `Installed`: false
- `Modules`: 단일 모듈 `ExNetworkRuntime` (Type: Runtime, LoadingPhase: Default)
- `Plugins`: 의존 플러그인 명시 — `OnlineSubsystem` (UE 표준)
  - Redpoint EOS Online Framework는 OnlineSubsystem 기반으로 자동 동작하므로 직접 의존 선언은 불필요

**가이드라인 준수:**
- 1.5 폴더 구조: 플러그인 최상위에 단독 배치

### 4.2 `Source/ExNetworkRuntime/ExNetworkRuntime.Build.cs`

**역할:** 런타임 모듈의 빌드 의존성 정의.

**Phase 1 의존 모듈 (최소화 — v2 수정):**
- `PublicDependencyModuleNames`:
  - `Core`, `CoreUObject`, `Engine` — UE 기본 (이것만 사용)
- `PrivateDependencyModuleNames`:
  - **(Phase 1 기준 비어 있음)**

**Phase 2 이후 추가될 의존성 (예고, 본 Phase에서는 추가하지 않음):**
- `OnlineSubsystem`, `OnlineSubsystemUtils` — 인증 진입 시
- `DeveloperSettings` — ProjectSettings 노출 시
- `Slate`, `SlateCore` — UI 연동 시 (Phase 5 추정)


**`PublicIncludePaths` 등록 패턴 (ExFrameWork 정책 핵심):**

ExCore의 `ExCoreRuntime.Build.cs`와 동일하게, 본 모듈도 모든 서브폴더를 `PublicIncludePaths`에 명시적으로 등록한다. 이는 ExFrameWork가 Public/Private 분리 없이도 외부 모듈에서 짧은 경로로 헤더를 참조할 수 있게 하는 핵심 메커니즘이다.

**Phase 1에서 등록할 경로:**
- `ModuleDirectory` (루트, Module.h 접근용)
- `Path.Combine(ModuleDirectory, "Core")`

**후속 Phase에서 추가될 경로 (예고):**
- `Strategies/` — Phase 2
- `Providers/`, `Providers/EOS/`, `Providers/Null/` — Phase 2~3
- `Match/` — Phase 3
- `Player/` — Phase 3
- `Events/` — Phase 2

**ExCore 의존성 관련 결정:**
- Phase 1에서는 ExCore에 의존하지 않음 (완전 독립 플러그인 원칙)
- 필요 시 후속 Phase에서 추가 검토 (예: `FExPopupDescriptor` 사용 시점)
- 의존 추가 시 의존성 방향: ExNetwork → ExCore (역방향 불가)

### 4.3 `Source/ExNetworkRuntime/ExNetworkRuntimeModule.h/cpp`

**역할:** 모듈 진입점. `IModuleInterface`를 구현하는 표준 보일러플레이트.

**ExCore와의 일관성:**
- ExCore는 `ExCoreRuntimeModule.h`와 `ExCoreRuntimeModule.cpp`를 모듈 루트에 평면 배치한다
- 본 모듈도 동일 패턴 — `ExNetworkRuntimeModule.h/cpp`를 `Source/ExNetworkRuntime/` 직속에 배치

**핵심 동작:**
- 표준 `IModuleInterface` 상속 클래스 정의 (`FExNetworkRuntimeModule`)
- `IMPLEMENT_MODULE(FExNetworkRuntimeModule, ExNetworkRuntime)` 매크로로 등록
- `StartupModule()`에서 로그 카테고리 정의 (`DEFINE_LOG_CATEGORY(LogExNetwork);`) 및 진입 로그 출력
- `ShutdownModule()`에서 종료 로그 출력
- Phase 1에서는 별도 초기화 코드 없음 (향후 Phase에서 EOS SDK 초기화 등 추가 가능)


### 4.4 `Core/ExOnlineSubsystem.h` 및 `Core/ExOnlineSubsystem.cpp`

**위치 정책:** `.h`와 `.cpp` 모두 `Source/ExNetworkRuntime/Core/` 폴더에 함께 배치 (ExFrameWork 정책 준수).

**역할:** 매칭/온라인 시스템의 진입점 클래스. GameInstanceSubsystem으로 구현되어 게임 전체 수명 동안 단일 인스턴스 유지.

**Phase 1 책임:**
- 빈 골격만 작성 (`UGameInstanceSubsystem` 상속)
- `Initialize` / `Deinitialize` 가상 함수 오버라이드 (로그만 출력)
- Phase 2 이후 점진적으로 책임 추가

**Phase 2 이후 책임 (예고):**
- 인증 Provider 보유 및 자동 로그인
- Server Strategy 자동 선택
- Lobby/Matchmaking Provider 관리
- 매칭 상태 머신 운영

**클래스 명세 (Phase 1):**
- 클래스: `UExOnlineSubsystem : public UGameInstanceSubsystem`
- 매크로: `UCLASS()`, `GENERATED_BODY()`
- 가시성: 모듈 외부 노출용 `EXNETWORKRUNTIME_API`
- 멤버: Phase 1에서는 멤버 없음
- 메서드:
  - `virtual void Initialize(FSubsystemCollectionBase& Collection) override;`
  - `virtual void Deinitialize() override;`
- 둘 다 본문은 `UE_LOG` 한 줄만 출력하여 정상 로드 확인

**가이드라인 준수:**
- 1.6 주석/문서: 클래스 헤더 및 메서드에 Doxygen 주석 작성
- 1.11 디버깅: Initialize/Deinitialize 진입 시 `UE_LOG(LogExNetwork, Log, ...)` 출력
- 1.10 생성자 제약: 본 Subsystem은 NewObject 호출 안 함, Initialize에서만 동작


### 4.5 `Core/ExNetworkLog.h`

**위치:** `Source/ExNetworkRuntime/Core/ExNetworkLog.h`

**역할:** ExNetwork 플러그인 전용 로그 카테고리 선언.

**핵심:**
- `DECLARE_LOG_CATEGORY_EXTERN(LogExNetwork, Log, All);` 헤더에 선언
- 대응 `DEFINE_LOG_CATEGORY(LogExNetwork);` 는 `ExNetworkRuntimeModule.cpp`에 배치
- 모든 ExNetwork 코드는 이 카테고리로 로그 출력
- 가이드라인 1.11(디버깅) 준수

**왜 별도 헤더로 분리하는가:**
- `ExNetworkRuntimeModule.h`에 두면 모듈 핵심 헤더에 로그 의존성이 생김
- 별도 헤더로 분리하면 `#include "Core/ExNetworkLog.h"` 만으로 어떤 cpp에서든 로그 사용 가능
- ExCore의 `Tags/ExGameplayTags.h` 등 단일 책임 헤더 분리 패턴과 일관

---

## 5. 외부 플러그인(Redpoint EOS) 통합

### 5.1 설치 위치

선택지:
- **(A)** 프로젝트 `Plugins/EOSOnlineFramework/` — 프로젝트별 독립, Git에 포함 가능
- **(B)** 엔진 `Engine/Plugins/Marketplace/` — 모든 프로젝트 공유, Git 제외

→ **추천: (A) 프로젝트 Plugins/** — 본 프로젝트 전용 운영, 빌드 머신과 개발 머신 모두 일관 보장

정확한 플러그인 폴더명은 다운로드한 Redpoint 패키지의 실제 명칭을 확인하여 사용 (`EOSOnlineFramework` 또는 `OnlineSubsystemRedpointEOS` 등 버전에 따라 상이).


### 5.2 활성화 방법

- `프로젝트.uproject` 파일에 Plugins 항목 추가:
  ```json
  {
    "Name": "EOSOnlineFramework",
    "Enabled": true
  }
  ```
- 또는 Editor 메뉴 → Edit → Plugins에서 활성화 후 에디터 재시작

### 5.3 Phase 1에서의 ini 변경 — 최소화 (v2 핵심 변경)

**v2에서는 Phase 1의 `DefaultEngine.ini` 변경을 최소화한다.** Redpoint 플러그인이 활성화된 상태로 빌드가 통과하는 것까지가 Phase 1의 목표이며, 다음 항목들은 **Phase 2 또는 Phase 3로 이동**되었다.

**Phase 1에서 추가 가능한 최소 ini (선택, 안전):**

Phase 1 검증 단계에서 Redpoint 플러그인이 자체 기본값을 가지므로 별도 ini 추가 없이도 빌드는 통과한다. 만약 Redpoint 플러그인 문서에서 "Plugin 활성화 후 반드시 추가해야 하는 ini 항목"이 명시되어 있으면 그 항목만 추가한다.

**Phase 1에서 추가하지 않는 항목 (v2 명시):**

- `[OnlineSubsystem] DefaultPlatformService=EOS` → **Phase 2로 이동** (인증 진입 시점)
- `[/Script/OnlineSubsystemEOS.OnlineSubsystemEOSConfig] Artifacts=(...)` Product 정보 → **Phase 2로 이동**
- `[/Script/Engine.GameEngine] +NetDriverDefinitions=(...)` NetDriver 등록 → **Phase 3로 이동** (실제 P2P 매치 동작 시점)
- `[/Script/OnlineSubsystemEOS.NetDriverEOS] bIsUsingP2PSockets=true` → **Phase 3로 이동**

### 5.4 Phase 1 ini 변경 정책 (v2 신설)

**원칙:** Phase 1에서는 다음 두 가지 외에는 `DefaultEngine.ini`를 건드리지 않는다.

1. (필요 시) Redpoint 플러그인 자체가 요구하는 최소 활성화 항목
2. 기존 ExFrameWork ini 설정과의 충돌이 발견된 경우의 최소 조정

이 정책의 의의:
- 기존 PIE 동작이 Phase 1에서 변경되지 않음 (회귀 차단)
- CICD 빌드 머신에서 EOS Product 정보 없이도 Phase 1 빌드 통과 가능
- Phase 2 진입 시 시크릿 관리 방식이 확정된 후에야 Product 정보 입력


### 5.5 Redpoint 플러그인 추가 설정

Redpoint EOS Online Framework는 자체 GameInstance 클래스(`RedpointGameInstance`) 등 추가 통합 옵션을 제공한다. 본 프로젝트의 기존 GameInstance와의 통합 방식은 **Phase 2 진입 시점에 결정**한다.

Phase 1에서는:
- Redpoint 플러그인 활성화만 수행
- 자체 GameInstance 강제 사용 여부 확인
- 본 프로젝트 GameInstance와의 충돌 가능성만 사전 파악 (구현은 Phase 2)

정확한 설정은 설치 후 플러그인 문서(`https://docs.redpoint.games/`)를 참고하여 보완한다.

---

## 6. 빌드 검증 (Build Validation)

### 6.1 Phase 1 검증 단계

다음 순서로 검증한다:

1. **Visual Studio 프로젝트 재생성** — `.uproject` 우클릭으로 sln 재생성
2. **에디터 빌드 통과** — `Development Editor` 타깃 빌드 통과
3. **에디터 실행 정상** — Plugin Manager에 ExNetwork 표시 확인
4. **모듈 로딩 로그 확인** — `LogExNetwork` 카테고리로 `ExNetworkRuntimeModule::StartupModule` 메시지 출력
5. **Subsystem 로딩 로그 확인** — `UExOnlineSubsystem::Initialize` 메시지 출력
6. **Redpoint 플러그인 로딩 확인** — 로그에 EOS Subsystem 초기화 메시지 출력 (단, OSS 기본값은 NULL 유지)
7. **빈 맵 PIE 실행** — PIE 정상 작동 확인 (게임플레이 영향 없음 검증)

### 6.2 Phase 1 검증 시 확인해야 할 것 vs 하지 말 것

**확인할 것:**
- 빌드 통과 (Compile Error 없음, Link Error 없음)
- 에디터 실행 안정성 (Crash 없음)
- 로그 메시지 정상 출력
- 기존 Runner 게임 정상 동작 (회귀 없음)

**확인하지 않을 것 (Phase 1 범위 외):**
- EOS 로그인 동작 (Phase 2)
- EOS Lobby 생성/검색 (Phase 3)
- P2P 통신 (Phase 3~4)
- Android 빌드 (별도 Phase)


### 6.3 검증 실패 시 처리

- 빌드 실패 → 의존성 모듈, `.Build.cs`, `.uplugin` 순서로 점검
- 플러그인 로딩 실패 → uplugin 매니페스트, `IMPLEMENT_MODULE` 매크로 확인
- include 경로 오류 → `PublicIncludePaths` 등록 누락 확인 (ExFrameWork 정책상 핵심)
- 가이드라인 1.7: 모든 실패 케이스에 명확한 에러 메시지로 디버깅 가능

### 6.4 CICD 빌드 머신 동기화

본 프로젝트는 `F:\wz\UE_CICD\SampleProject` 경로의 별도 빌드 머신을 사용한다. Phase 1 완료 후 다음을 동기화:

- `Plugins/ExNetwork/` 폴더 전체
- Redpoint EOS Online Framework 플러그인
- `.uproject` 활성화 항목 변경

CICD에서 EOS Product 정보 없이도 Phase 1 빌드는 통과해야 한다 (v2 핵심 의의).

---

## 7. 아키텍처 요약 문서 갱신 (v2)

### 7.1 현재 상태

Phase 1 v1 작업으로 다음 두 문서가 이미 작성되어 있다:

- `Plugins/ExNetwork/Md/ExNetwork_Architecture_Summary.md`
- `Plugins/ExNetwork/Md/ExNetwork_Module_Layout.md`

### 7.2 v2에서 갱신할 내용

본 v2 Plan과 함께 위 두 문서도 갱신된다 (별도 작업으로 본 Plan과 병행):

- Public/Private 분리 표기 완전 제거
- 폴더 구조를 ExFrameWork 정책에 맞는 평면 구조로 재작성
- "본 플러그인은 ExFrameWork 정책에 따라 Public/Private 분리를 사용하지 않는다" 명시 (정책 문서 역할)
- 모듈명을 `ExNetworkRuntime`으로 통일
- Build.cs의 PublicIncludePaths 등록 패턴 명시

### 7.3 작성 원칙 (v1 유지)

- 간략하게 (1페이지 내외, 핵심만)
- 예시 코드 미포함 (가이드라인의 학습 사항 준수 — 의도 중심)
- 다이어그램은 ASCII 표현 (외부 의존성 없음)
- 링크 활용 (상세 내용은 Plan 문서로 링크)


---

## 8. 구현 단계 (Implementation Steps)

Phase 1은 단일 Phase이지만, 내부적으로 다음 4단계로 분할하여 진행한다.

### Step 1 — 기존 플러그인 폴더 골격 보강 (v2 정정)

**v2 변경 사항:** v1의 "신규 디렉토리 생성"에서 "기존 폴더 골격 보강"으로 정정. `Plugins/ExNetwork/Md/`는 이미 존재하므로 보존하고, 누락된 항목만 추가.

**작업:**
- `Plugins/ExNetwork/` 폴더는 이미 존재 (Md/ 포함) — 보존
- `ExNetwork.uplugin` 신규 작성
- `Resources/Icon128.png` 임시 아이콘 배치 (디폴트 아이콘 복사)
- `Config/FilterPlugin.ini` 빈 파일 생성

**덮어쓰기 정책:** 기존 `Md/` 폴더 파일들은 절대 덮어쓰지 않음 (Step 4에서 별도 갱신). Source 폴더는 신규 생성.

**검증:** Editor 재시작 후 Plugins 메뉴에서 ExNetwork 표시 확인

### Step 2 — 런타임 모듈 골격 작성

**작업:**
- `Source/ExNetworkRuntime/` 폴더 신규 생성 (서브폴더 분리 없음, 평면 구조)
- `ExNetworkRuntime.Build.cs` 작성 (의존성 최소화, PublicIncludePaths 등록)
- `ExNetworkRuntimeModule.h/cpp` 작성 (모듈 클래스 + IMPLEMENT_MODULE)
- `Core/ExNetworkLog.h` 로그 카테고리 선언
- `Core/ExOnlineSubsystem.h/cpp` 빈 골격 작성

**파일 배치 정책 (v2 핵심):**
- `.h`와 `.cpp`는 같은 폴더에 함께 배치 (ExFrameWork 정책)
- `Public/`, `Private/` 폴더 분리 사용하지 않음

**검증:** Visual Studio 빌드 통과, 에디터 실행 시 로그 출력 확인

### Step 3 — Redpoint EOS 플러그인 활성화 (ini 변경 최소)

**v2 변경 사항:** Product 정보 입력은 Phase 2로 이동. 본 Step에서는 활성화만 수행.

**작업:**
- Redpoint 플러그인을 `Plugins/EOSOnlineFramework/` (정확한 명칭은 다운로드 후 확인)에 배치
- `프로젝트.uproject`에 활성화 항목 추가
- (필요 시) Redpoint 플러그인이 요구하는 최소 ini 항목만 추가

**Phase 1에서 추가하지 않는 ini (v2 명시):**
- `DefaultPlatformService=EOS`
- Product 정보 Artifacts
- NetDriverDefinitions

**검증:** 에디터 실행 시 Redpoint 플러그인 정상 로드 확인, 기존 PIE 동작 회귀 없음 확인


### Step 4 — 아키텍처 문서 v2 갱신

**작업:**
- 기존 `Plugins/ExNetwork/Md/ExNetwork_Architecture_Summary.md` 갱신
- 기존 `Plugins/ExNetwork/Md/ExNetwork_Module_Layout.md` 갱신
- Public/Private 표기 제거, 폴더 구조 평면화 반영

**갱신 정책:** 직접 수정(덮어쓰기). v1 문서를 그대로 두지 않고 정확한 정책으로 일원화.

**검증:** 문서 작성 완료 후 주인님 검토

---

## 9. 잠재 리스크 및 대응 (Risk Analysis)

| 리스크 | 심각도 | 대응 방안 |
|---|---|---|
| Redpoint 플러그인이 UE 5.7.4 호환성 문제 | 중 | Free Edition은 UE 최신 +1버전 지원, 5.7.4 출시 시점 검토. 문제 시 Paid Edition 또는 대기 |
| Redpoint 플러그인이 자체 GameInstance 강제 사용 가능성 | 중 | Phase 1에서는 충돌 가능성만 사전 파악. Phase 2에서 본 프로젝트 GameInstance와의 통합 방식 결정 |
| EOS Subsystem이 OnlineSubsystemEOS와 OnlineSubsystemEOSPlus 중 어느 것을 등록하는지 혼선 | 중 | Redpoint 문서 확인 후 정확한 설정 적용 (Phase 2 진입 전) |
| 안드로이드 빌드 시 EOS Android SDK 추가 설정 필요 | 중 | 본 Phase 1에서는 Windows Editor만 검증, Android는 별도 Phase로 분리 |
| 기존 빌드 시스템(F: 드라이브 CICD)에 Redpoint 플러그인 미설치 | 중 | Phase 1 검증 시 동기화 필요. CICD 머신에도 동일 플러그인 배치 |
| **(v2 신설)** Public/Private 분리 안 하는 정책을 외부 협업자가 모를 위험 | 저 | 아키텍처 문서에 정책 명시, 가이드라인 1.5에 항목 추가 검토 |
| **(v2 신설)** NetDriverDefinitions 누락으로 Phase 3에서 P2P 미동작 | 중 | Phase 3 Plan 첫 페이지에 NetDriver 등록을 핵심 항목으로 명기 (예고) |
| **(v2 신설)** 시크릿 관리 방식 미확정 상태로 Phase 2 진입 | 고 | §13 승인 조건에 필수 게이트로 명시 |


---

## 10. 변경하지 않는 것 (Out of Scope)

본 Phase 1에서는 다음을 변경/추가하지 않는다:

- 기존 ExCore의 멀티플레이 시작 동기화 로직
- 기존 GameInstance 상속 구조 (Redpoint 충돌 시 Phase 2에서 결정)
- 기존 다른 플러그인의 Config
- `DefaultEngine.ini`의 `DefaultPlatformService` (v2 명시 — Phase 2로 이동)
- EOS Product 정보 입력 (v2 명시 — Phase 2로 이동)
- NetDriverDefinitions 등록 (v2 명시 — Phase 3로 이동)
- 게임플레이 로직 (Pawn, GameMode, GameState 등)
- 안드로이드 빌드 설정 (별도 Phase로 분리)
- UI 위젯 (Plan 5에서 진행)

---

## 11. 후속 Plan 예고 (Roadmap Preview)

본 Phase 1 완료 후 진행될 후속 Plan을 간략히 정리한다.

### Plan 2 — 인증 + 인터페이스 코어 구축
- `IExAuthProvider` 인터페이스 정의
- `ExEOSAuthProvider` (Device ID 자동 로그인) 구현
- `IExNetServerStrategy` 인터페이스 정의
- `ExListenServerStrategy`, `ExDedicatedServerStrategy` 골격
- 환경 자동 감지 (UE_SERVER, NetMode)
- `ExOnlineSubsystem` 부팅 시 자동 인증 흐름
- **(v2 명시 이관 항목)** `DefaultEngine.ini`에 `DefaultPlatformService=EOS` 추가
- **(v2 명시 이관 항목)** Product 정보 Artifacts 입력
- **(v2 필수 게이트)** 시크릿 관리 방식 확정 후 진입

### Plan 3 — Lobby Provider + Quick Match 매칭
- `IExLobbyProvider` 인터페이스
- `ExEOSLobbyProvider` 구현 (생성/검색/참가)
- `FExMatchConfig`, `FExMatchHandle`, `FExMatchAttributes` USTRUCT
- Quick Match 흐름 (빈 Lobby 검색 → 없으면 생성)
- 매칭 상태 머신
- **(v2 명시 이관 항목)** `NetDriverDefinitions` 등록 (`[/Script/Engine.GameEngine]` 섹션)
- **(v2 명시 이관 항목)** `[/Script/OnlineSubsystemEOS.NetDriverEOS] bIsUsingP2PSockets=true`

### Plan 4 — Lobby → Game 전환
- Listen Server: Lobby Owner의 ServerTravel + 멤버 ClientTravel
- 기존 ExCore `Match_WaitingForPlayers` 흐름과 결합
- Lobby Destroy 및 게임 종료 후 Lobby 복귀

### Plan 5 — Matchmaking UI
- 매칭 시작 버튼 / 대기 화면 / 매칭 완료 알림
- Lobby 멤버 표시 (실시간 갱신)
- 기존 `UExPopupWidget` 활용

### Plan 6+ — 매칭 방식 확장
- Phase 6: Invite Code Match
- Phase 7: Friend Invite Match
- Phase 8: Skill-based Matchmaking
- Phase 9: Region-based / Custom Filter

각 Plan은 본 Plan과 동일한 형식(보고/승인/외부 AI 검토)으로 진행된다.


---

## 12. 가이드라인 준수 체크리스트 (v2 — 3종 시점 분리)

체크 기호 의미:
- `[x]` 본 Plan 작성 시점에 충족
- `[→]` Phase 1 완료 시 충족 예정
- `[ ]` 후속 Phase에서 충족 예정

| 가이드라인 | 시점 | 상태 |
|---|---|---|
| 1.2 명명 규칙: 플러그인/모듈/클래스 모두 Ex 접두사 | Phase 1 완료 시 | `[→]` |
| 1.4 단일 책임: 플러그인이 매칭/온라인 단일 책임만 담당 | 본 Plan에서 설계 명시 | `[x]` |
| 1.5 폴더 구조: ExFrameWork 정책 준수 (Public/Private 분리 없음) | Phase 1 완료 시 | `[→]` |
| 1.6 주석/문서: 신규 클래스 Doxygen 주석, Md/ 문서 갱신 | Phase 1 완료 시 | `[→]` |
| 1.7 검증/체크: 보안 정보 분리 관리 (시크릿 게이트), 빌드 검증 단계 명시 | 본 Plan에서 정책 명시 | `[x]` |
| 1.8 서버 권한: Strategy Pattern으로 서버 모델 추상화 | Plan 2 이후 | `[ ]` |
| 1.10 생성자 제약: Subsystem은 NewObject 호출 안 함 | Phase 1 완료 시 | `[→]` |
| 1.11 디버깅: 전용 로그 카테고리 `LogExNetwork` 정의 | Phase 1 완료 시 | `[→]` |
| 3.1 보고/승인: 본 Plan이 사전 보고 | 본 Plan 자체 | `[x]` |
| 3.3 점진적 검증: Step 1~4 분할, 안드로이드 별도 Phase | 본 Plan에서 분할 명시 | `[x]` |
| 4.1 의존성 방향: ExNetwork → ExCore (한 방향), ExCore는 ExNetwork를 모름 | 본 Plan에서 원칙 명시 | `[x]` |

---

## 13. 승인 요청 및 필수 게이트

### 13.1 본 Plan 승인 후 즉시 진행 가능 항목

- Step 1: 기존 폴더 골격 보강 (EOS 정보 무관)
- Step 2: 런타임 모듈 골격 작성 (EOS 정보 무관)
- Step 4: 아키텍처 문서 v2 갱신


### 13.2 Step 3(Redpoint 활성화) 진입 전 사전 조건

- Redpoint EOS Online Framework 입수 (Free 또는 Paid)
- 본 프로젝트의 `Plugins/` 또는 엔진 `Engine/Plugins/Marketplace/`에 배치
- UE 5.7.4 호환 확인

### 13.3 Phase 2 진입 전 필수 게이트 (v2 신설)

다음 항목이 **모두 확정**되어야 Phase 2 진입 가능:

1. **EOS Developer Portal 가입 완료**
2. **Product 발급 정보 6종 확보** (Product ID, Sandbox ID, Deployment ID, Client ID, Client Secret, Encryption Key)
3. **시크릿 관리 방식 확정**
   - `.gitignore` 항목 추가
   - 로컬 ini vs CI 환경변수 주입 방식 결정
   - 팀 협업 시 시크릿 공유 절차 결정 (현재는 1인 개발이므로 단순화 가능)
4. **Redpoint GameInstance 통합 방식 결정** (강제 사용 여부 확인 후)

이 게이트가 미충족 상태에서 Phase 2 진입을 시도하면 안 된다.

### 13.4 v2 작성 의의

- 외부 AI 피드백 8건 + ExFrameWork 정책(Public/Private 미사용) + 추가 발견 2건 반영
- Phase 1 범위를 명확히 축소하여 회귀 리스크 차단
- 시크릿 관리, NetDriver 등록 등 누락되기 쉬운 항목을 후속 Phase로 명시 이관
- 아키텍처 문서와 일관된 정책 명시 (Public/Private 분리 없음)

수정/추가가 필요한 항목이 있으면 알려주십시오. 승인하시면 Step 1부터 순차 구현에 착수하겠습니다.
