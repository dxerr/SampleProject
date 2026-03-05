# ExFrameWork: 멀티플레이어 대응 Flow 및 시스템 아키텍처 설계서

> **버전:** v2.0  
> **대상 엔진:** Unreal Engine 5  
> **프로젝트:** ExFrameWork  
> **작성일:** 2026-03-03  

---

## 1. 아키텍처 개요 및 핵심 원칙

### 1.1 목표
확장성이 뛰어나고 이벤트 주도적(Event-driven)인 멀티플레이어 게임 아키텍처를 구축한다.
데디케이티드 서버 및 리슨 서버를 완벽하게 지원하며, 언리얼 엔진의 클라이언트-서버 모델(권한/Authority, RPC, 리플리케이션)을 엄격하게 준수한다.

### 1.2 프로젝트 레이어 구조

ExFrameWork는 GameFeatures 플러그인 아키텍처를 채택하며, 개발 레이어를 명시적으로 분리한다.
ExCore 플러그인은 항상 활성화 상태를 전제로 하며, 비활성화되는 경우는 고려하지 않는다.

```
ExFrameWork/                        ← 메인 모듈 (앱 레벨, Travel 불가침 영역)
│
├── Plugins/GameFeatures/
│   ├── ExCore/                     ← 핵심 베이스 클래스 레이어 (항상 활성화)
│   │   ├── GameMode/GameState 베이스 클래스
│   │   ├── PlayerState 베이스 클래스
│   │   └── 공용 컴포넌트/유틸리티
│   │
│   ├── ExRunnerPlay/               ← 러너 게임 피처 (GameMode 오버라이드)
│   ├── ExBattlePlay/               ← (향후) 배틀 게임 피처
│   └── Ex???Play/                  ← (향후) 추가 게임 피처
```

**레이어 배치 규칙:**

| 클래스 | 배치 위치 | 이유 |
|---|---|---|
| `UGameFlowSubsystem` | **ExCore** | 앱 레벨 Flow이지만, ExCore가 항상 활성화 전제이므로 Core에 배치하여 개발 레이어 일관성을 유지한다. |
| `UBackendCommunicationSubsystem` | **ExCore** | 외부 통신은 게임 모드에 무관한 공용 기능이다. |
| `AExGameModeBase` | **ExCore** | 모든 게임 피처의 GameMode가 상속받는 베이스 클래스. |
| `AExGameStateBase` | **ExCore** | 모든 게임 피처의 GameState가 상속받는 베이스 클래스. |
| `AExPlayerStateBase` | **ExCore** | 모든 게임 피처의 PlayerState가 상속받는 베이스 클래스. |
| `AExRunnerGameMode` | **ExRunnerPlay** | 러너 전용 매치 로직, ExGameModeBase를 상속. |

### 1.3 5대 핵심 원칙

1. **글로벌 Flow 제어:** `GameInstanceSubsystem`과 `GameplayTags`를 사용하여 전역 상태(Boot → IDP → Lobby → InGame)를 관리한다.
2. **맵 종속 규칙 (매치 룰):** `AGameModeBase`(서버 전용)와 `AGameStateBase`(모든 클라이언트로 복제됨)를 통해 관리한다.
3. **플레이어 데이터:** `APlayerState`에 저장하여 모든 유저에게 복제(Replicate)한다.
4. **모듈형 시스템:** 기능 로직은 `UActorComponent` 형태로 제작하되, 부착 대상(PlayerController/PlayerState/Pawn 등)에 종속되지 않도록 설계한다. 외부 통신 모듈은 `UGameInstanceSubsystem`으로 제작한다.
5. **이벤트 주도 (Event-Driven):** 시스템 간 통신은 델리게이트(Delegates) 또는 `GameplayMessageSubsystem`을 사용한다. 강결합(Tight Coupling)을 금지한다.

### 1.4 엄격한 제약 사항 (모든 단계에 적용)

**리플리케이션 규칙:**
- `UPROPERTY(Replicated)` 또는 `UPROPERTY(ReplicatedUsing=)` 변수를 추가할 때는 반드시 `GetLifetimeReplicatedProps` 함수에 등록해야 한다.
- `GameMode`는 서버에만 존재한다. 클라이언트 컨텍스트(UI 위젯, 클라이언트 사이드 Tick 등)에서 `GetWorld()->GetAuthGameMode()`를 절대 호출하지 마라. `nullptr` 반환으로 크래시가 발생한다. 클라이언트에서는 항상 `GameState`를 사용하라.
- `UWorldSubsystem`은 Actor가 아니므로 `HasAuthority()`를 사용할 수 없다. 대신 `GetWorld()->GetNetMode()`를 사용하라.

**UI 규칙:**
- UI는 `PlayerState`나 `GameState`에서 발생하는 데이터 변경(`OnRep` 함수 또는 델리게이트)에 의해 수동적으로 구동되어야 한다.
- 서버가 위젯(Widget)을 생성하려고 시도해서는 절대 안 된다.
- UI 업데이트 패턴: `OnRep_변수()` → 로컬 델리게이트 Broadcast → HUD/Widget이 해당 델리게이트를 구독하여 반응.

**코딩 규칙:**
- 매직 넘버를 금지한다. 모든 게임플레이 수치는 `UPROPERTY(EditAnywhere, Category="...")` 또는 `UPROPERTY(EditDefaultsOnly, Category="...")`로 노출하라.
- 컴포넌트 초기화 타이밍: `PostInitializeComponents`에서 핵심 참조를 설정하고, `BeginPlay`에서 게임 로직을 시작하라. `TickComponent` 내 Lazy Init 패턴은 사용하지 마라. 재시도가 필요하면 `GetWorldTimerManager().SetTimer`를 사용하라.
- 델리게이트 해제 규칙: `BeginPlay` 또는 `PostInitializeComponents`에서 바인딩한 모든 델리게이트는 반드시 `EndPlay`에서 해제(Unbind)하라. 이를 누락하면 dangling pointer 크래시의 원인이 된다.

---

## 2. 상태 계층 정의 (Flow vs Match)

이 프로젝트는 두 가지 독립적인 상태 계층을 사용한다. 혼동을 방지하기 위해 명확히 분리한다.

### 2.1 앱 레벨 상태 (GameFlowSubsystem 관할)

애플리케이션의 생명주기를 나타내며, 레벨(맵) 전환을 포함한다.

```
Flow.Boot → Flow.Auth.IDP → Flow.Lobby → Flow.InGame → Flow.Lobby (복귀)
```

| 태그 | 설명 | 담당 |
|---|---|---|
| `Flow.Boot` | 엔진 초기화, 에셋 로딩 | GameFlowSubsystem |
| `Flow.Auth.IDP` | 로그인/인증 화면 | GameFlowSubsystem + BackendCommunicationSubsystem |
| `Flow.Lobby` | 매치메이킹, 세션 탐색 | GameFlowSubsystem |
| `Flow.InGame` | 게임 맵 로드 완료 | GameFlowSubsystem → 이후 GameState로 제어권 이관 |

### 2.2 매치 레벨 상태 (GameState 관할)

`Flow.InGame` 진입 이후, 게임 맵 안에서의 매치 진행 상황을 나타낸다.

```
Match.WaitingForPlayers → Match.Countdown → Match.Playing → Match.PostMatch
```

| 태그 | 설명 | 담당 |
|---|---|---|
| `Match.WaitingForPlayers` | 모든 플레이어 로딩 대기 | GameMode(서버)가 설정 → GameState로 복제 |
| `Match.Countdown` | 카운트다운 진행 | GameMode(서버)가 설정 → GameState로 복제 |
| `Match.Playing` | 게임 플레이 중 | GameMode(서버)가 설정 → GameState로 복제 |
| `Match.PostMatch` | 결과 화면, 정산 | GameMode(서버)가 설정 → GameState로 복제 |

### 2.3 상태 전이 유효성 검증

허용되지 않는 상태 점프를 방지하기 위해 전이 맵(Transition Map)을 사용한다.

```
허용되는 전이 (Flow):
  Flow.Boot         → Flow.Auth.IDP
  Flow.Auth.IDP     → Flow.Lobby
  Flow.Auth.IDP     → Flow.Boot          (인증 실패 시 재시도)
  Flow.Lobby        → Flow.InGame
  Flow.Lobby        → Flow.Auth.IDP      (로그아웃)
  Flow.InGame       → Flow.Lobby         (매치 종료 후 복귀)

허용되는 전이 (Match):
  Match.WaitingForPlayers → Match.Countdown
  Match.Countdown         → Match.Playing
  Match.Playing           → Match.PostMatch
  Match.PostMatch         → Match.WaitingForPlayers  (재매치 시)
```

**금지되는 전이 예시:** `Flow.Boot → Flow.InGame` (로그인과 로비를 건너뜀), `Match.WaitingForPlayers → Match.Playing` (카운트다운 건너뜀).

---

## 3. 1단계 구현: 글로벌 게임 Flow 서브시스템

> **실행 지시:** 이 단계를 먼저 구현하고, 사용자의 승인을 받은 후 다음 단계로 진행하라.

### 3.1 클래스 정보

| 항목 | 값 |
|---|---|
| 클래스명 | `UExGameFlowSubsystem` |
| 부모 클래스 | `UGameInstanceSubsystem` |
| 배치 위치 | `Plugins/GameFeatures/ExCore/Source/ExCoreRuntime/Subsystems/` |
| 모듈 | ExCore |

### 3.2 핵심 요구사항

#### 3.2.1 상태 관리
- `FGameplayTag CurrentFlowState`를 멤버 변수로 보유한다.
- 상태 변경 함수: `UFUNCTION(BlueprintCallable, Category="ExFlow") void SetFlowState(FGameplayTag NewState);`
- `SetFlowState` 내부에서 반드시 전이 유효성을 검증한다. 허용되지 않는 전이는 `UE_LOG`로 경고를 출력하고 무시한다.
- 허용된 전이 맵은 `TMap<FGameplayTag, TArray<FGameplayTag>> AllowedTransitions`로 관리하며, `Initialize()` 시점에 설정한다.

#### 3.2.2 델리게이트
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFlowStateChanged, const FGameplayTag&, OldState, const FGameplayTag&, NewState);

UPROPERTY(BlueprintAssignable, Category="ExFlow")
FOnFlowStateChanged OnFlowStateChanged;
```

#### 3.2.3 Travel 처리 — 서버/클라이언트 분기 및 한계 극복 (핵심)
- **`UGameInstanceSubsystem`의 한계:** 이 서브시스템은 클라이언트/서버 각각 독립적으로 존재하며 네트워크 복제(Replication)를 지원하지 않습니다. 단순히 상태를 바꾼다고 클라이언트의 로컬 Flow 상태가 연동되지 않습니다.
- **클라이언트 동기화 구조:** 클라이언트 동기화를 보장하기 위해, `GameMode`의 권한을 통한 `GameState`의 OnRep 델리게이트 또는 `PlayerController`에 Client RPC 브릿지를 생성하여, 전역 Flow State 변경을 클라이언트 로컬 `GameFlowSubsystem`에 알리는 설계가 필수입니다.
- `SetFlowState`에서 Travel이 필요한 상태 전이가 감지될 때:
  - **리슨 서버 (방장) / 데디케이티드 서버 매치 이동 (`Flow.Lobby → Flow.InGame`):** 이미 접속된 클라이언트들을 데리고 맵을 변경할 때는 `GetWorld()->ServerTravel`을 호출합니다.
  - **클라이언트 세션 접속 (`Flow.Lobby → Flow.InGame` 타 서버 접속 시):** 클라이언트 입장에서 완전히 새로운 매치에 접속할 때는 데디서버와 네트워크 연결이 없었던 상태이므로 `ClientTravel` (또는 `OpenLevel`에 의한 Session Join)이 필요함을 내부적으로 구분하여 설계합니다.
- Travel 요청 방식 (권장): 서브시스템은 Travel 의도만 브로드캐스트하고, 실제 `ServerTravel` 호출은 GameMode가 수행하도록 델리게이트(`FOnRequestTravel`)를 구독시킵니다.

```cpp
// Travel 요청 델리게이트 (Flow 서브시스템이 선언)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRequestTravel, const FString&, MapURL);

UPROPERTY(BlueprintAssignable, Category="ExFlow")
FOnRequestTravel OnRequestTravel;
```

#### 3.2.4 포함해야 할 헤더
```cpp
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "ExGameFlowSubsystem.generated.h"
```

### 3.3 구현 시 주의사항
- `GameInstanceSubsystem`은 레벨(맵)이 변경되어도 파괴되지 않는다. 따라서 `CurrentFlowState`는 Travel 이후에도 유지된다.
- `Initialize()` 오버라이드에서 전이 맵을 설정하고, 초기 상태를 `Flow.Boot`으로 설정한다.
- `Deinitialize()` 오버라이드에서 모든 델리게이트 바인딩을 정리한다.

### 3.4 GameplayTag 네이티브 등록
- 프로젝트에서 사용할 태그를 `NativeGameplayTags`로 등록하는 전용 헤더(`ExFlowTags.h`)를 만든다.

```cpp
// ExFlowTags.h
#pragma once
#include "NativeGameplayTags.h"

namespace ExFlowTags
{
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Flow_Boot);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Flow_Auth_IDP);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Flow_Lobby);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Flow_InGame);
}
```

```cpp
// ExFlowTags.cpp
#include "ExFlowTags.h"

namespace ExFlowTags
{
    UE_DEFINE_GAMEPLAY_TAG(Flow_Boot,     "Flow.Boot");
    UE_DEFINE_GAMEPLAY_TAG(Flow_Auth_IDP, "Flow.Auth.IDP");
    UE_DEFINE_GAMEPLAY_TAG(Flow_Lobby,    "Flow.Lobby");
    UE_DEFINE_GAMEPLAY_TAG(Flow_InGame,   "Flow.InGame");
}
```

### 3.5 검증 체크리스트
- [ ] `SetFlowState`에서 유효하지 않은 전이를 시도하면 로그 경고가 출력되고 상태가 변경되지 않는다.
- [ ] 상태가 변경되면 `OnFlowStateChanged` 델리게이트가 Old/New 태그와 함께 브로드캐스트된다.
- [ ] `Flow.Lobby → Flow.InGame` 전이 시, 서버 컨텍스트에서 `OnRequestTravel`이 브로드캐스트된다.
- [ ] 클라이언트 컨텍스트에서는 Travel이 직접 호출되지 않고 UI 전환만 발생한다.
- [ ] `Deinitialize()`에서 모든 바인딩이 해제된다.

---

## 4. 2단계 구현: 서버 권한 및 매치 Flow (GameMode & GameState 베이스)

> **실행 지시:** 1단계 승인 후 이 단계를 구현하라. 사용자의 승인을 받은 후 3단계로 진행하라.

### 4.1 설계 의도 — 확장성 구조

ExCore에 베이스 클래스를 두고, 각 GameFeature 플러그인에서 필요에 따라 오버라이드하여 확장하는 구조를 만든다.

```
ExCore (베이스)                     GameFeature (확장)
─────────────────                  ──────────────────
AExGameModeBase          ←────     AExRunnerGameMode   (ExRunnerPlay)
AExGameStateBase         ←────     AExRunnerGameState   (ExRunnerPlay)
AExPlayerStateBase       ←────     AExRunnerPlayerState (ExRunnerPlay)

                                   AExBattleGameMode   (ExBattlePlay, 향후)
                                   ...
```

### 4.2 AExGameModeBase (서버 전용)

| 항목 | 값 |
|---|---|
| 클래스명 | `AExGameModeBase` |
| 부모 클래스 | `AGameModeBase` |
| 배치 위치 | `Plugins/GameFeatures/ExCore/Source/ExCoreRuntime/GameModes/` |

#### 4.2.1 핵심 요구사항

**매치 페이즈 관리 (서버 권한):**
- `SetMatchPhase(FGameplayTag NewPhase)` 함수를 제공한다.
- 이 함수는 `GameState`의 `CurrentMatchPhase`를 변경한다. (GameMode → GameState로의 단방향 제어)
- 매치 페이즈 전이 유효성을 검증한다. (허용된 전이 맵 사용, Flow 서브시스템과 동일한 패턴)

**Travel 수신 인터페이스:**
- `BeginPlay`에서 `UExGameFlowSubsystem::OnRequestTravel`을 구독한다.
- 콜백 내에서 `GetWorld()->ServerTravel(MapURL)`을 호출한다.
- `EndPlay`에서 구독을 해제한다.

**플레이어 로딩 관리:**
- `PostLogin(APlayerController*)` 오버라이드: 접속한 플레이어를 추적한다.
- `HandleStartingNewPlayer(APlayerController*)` 오버라이드: 캐릭터 스폰 위치를 지정한다.
- 모든 플레이어가 로딩을 마쳤는지 확인하는 `CheckAllPlayersReady()` 함수를 제공한다.

**하위 클래스가 오버라이드할 가상 함수:**
```cpp
// 매치 시작 시 호출 (하위에서 게임별 초기화 수행)
virtual void OnMatchStarted();

// 매치 종료 시 호출
virtual void OnMatchEnded();

// 캐릭터 스폰 위치 결정 (하위에서 게임별 로직 수행)
virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
```

**절대 금지:**
- UI 관련 로직을 이 클래스에 넣지 마라.
- 클라이언트에서 접근 가능한 데이터를 직접 보유하지 마라. 복제가 필요한 데이터는 GameState에 넣어라.

#### 4.2.2 포함해야 할 헤더
```cpp
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayTagContainer.h"
#include "ExGameModeBase.generated.h"
```

### 4.3 AExGameStateBase (복제됨)

| 항목 | 값 |
|---|---|
| 클래스명 | `AExGameStateBase` |
| 부모 클래스 | `AGameStateBase` |
| 배치 위치 | `Plugins/GameFeatures/ExCore/Source/ExCoreRuntime/GameModes/` |

#### 4.3.1 핵심 요구사항

**복제되는 매치 페이즈:**
```cpp
UPROPERTY(ReplicatedUsing = OnRep_MatchPhase, BlueprintReadOnly, Category="ExMatch")
FGameplayTag CurrentMatchPhase;
```

**OnRep 및 로컬 델리게이트 패턴:**
```cpp
// 로컬 델리게이트 (UI가 구독)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMatchPhaseChanged, const FGameplayTag&, OldPhase, const FGameplayTag&, NewPhase);

UPROPERTY(BlueprintAssignable, Category="ExMatch")
FOnMatchPhaseChanged OnMatchPhaseChanged;

// OnRep 구현
UFUNCTION()
void OnRep_MatchPhase(const FGameplayTag& OldPhase);
```

`OnRep_MatchPhase` 구현:
1. `OldPhase`를 파라미터로 받는다. (`ReplicatedUsing`에서 이전 값을 자동 전달받으려면 함수 시그니처에 이전 값 파라미터를 추가한다.)
2. 로컬 `OnMatchPhaseChanged` 델리게이트를 브로드캐스트한다.
3. 이 델리게이트를 HUD/Widget이 구독하여 UI를 업데이트한다.
4. **난입(Join-in-Progress) 예외 처리:** 게임 중간에 게임에 들어오는 클라이언트의 경우, 서버의 현재 값이 초기화 기본값과 같다면 `OnRep`이 한 번도 호출되지 않을 수 있습니다. 따라서 로컬 `BeginPlay()` 타이밍에 현재 `CurrentMatchPhase`를 기반으로 로컬 UI 델리게이트를 1회 강제로 `Broadcast` 해주는 초기화 보호(Init Guard) 로직을 반드시 추가해야 합니다.

**하위 클래스가 오버라이드할 가상 함수:**
```cpp
// 매치 페이즈 변경 시 하위에서 추가 처리 가능
virtual void HandleMatchPhaseChanged(const FGameplayTag& OldPhase, const FGameplayTag& NewPhase);
```

**GetLifetimeReplicatedProps 필수:**
```cpp
void AExGameStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AExGameStateBase, CurrentMatchPhase);
}
```

#### 4.3.2 포함해야 할 헤더
```cpp
#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "GameplayTagContainer.h"
#include "Net/UnrealNetwork.h"
#include "ExGameStateBase.generated.h"
```

### 4.4 매치 태그 네이티브 등록

```cpp
// ExMatchTags.h
#pragma once
#include "NativeGameplayTags.h"

namespace ExMatchTags
{
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Match_WaitingForPlayers);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Match_Countdown);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Match_Playing);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Match_PostMatch);
}
```

### 4.5 검증 체크리스트
- [ ] `AExGameModeBase::SetMatchPhase()`가 호출되면 `AExGameStateBase::CurrentMatchPhase`가 변경된다.
- [ ] `CurrentMatchPhase` 변경이 모든 클라이언트에 복제된다.
- [ ] 클라이언트에서 `OnRep_MatchPhase`가 호출되고, `OnMatchPhaseChanged` 델리게이트가 브로드캐스트된다.
- [ ] 클라이언트 코드에서 `GetAuthGameMode()`를 호출하는 곳이 없다.
- [ ] `OnRequestTravel` 구독이 `BeginPlay`에서 바인딩되고 `EndPlay`에서 해제된다.
- [ ] 유효하지 않은 매치 페이즈 전이를 시도하면 로그 경고가 출력된다.

---

## 5. 3단계 구현: 플레이어 데이터 및 모듈형 컴포넌트

> **실행 지시:** 2단계 승인 후 이 단계를 구현하라. 사용자의 승인을 받은 후 4단계로 진행하라.

### 5.1 AExPlayerStateBase

| 항목 | 값 |
|---|---|
| 클래스명 | `AExPlayerStateBase` |
| 부모 클래스 | `APlayerState` |
| 배치 위치 | `Plugins/GameFeatures/ExCore/Source/ExCoreRuntime/Player/` |

#### 5.1.1 핵심 요구사항

**복제되는 점수 (엔진 내장 변수 활용):**
언리얼의 기본 `APlayerState`에는 이미 리플리케이트 설정이 완료된 `float Score` 변수와, 관련 OnRep 가상 함수(`OnRep_Score()`, `GetScore()`)가 내장되어 있습니다. 메모리/네트워크 낭비를 피하고 엔진과의 호환성을 극대화하기 위해 이를 재사용합니다.

```cpp
// 내장 Score 변수의 동기화 알림 가상함수를 오버라이드
virtual void OnRep_Score() override;

// 서버에서만 호출 가능한 점수 변경 함수 (내장 세터 활용)
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="ExPlayer")
void AddScore(float Amount);
```

**OnRep → 로컬 델리게이트 패턴:**
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnScoreChanged, float, OldScore, float, NewScore);

UPROPERTY(BlueprintAssignable, Category="ExPlayer")
FOnScoreChanged OnScoreChangedDelegate; // 내장 OnScoreChanged가 없으므로 Custom 명칭 사용
```

**GetLifetimeReplicatedProps 필수.**

#### 5.1.2 포함해야 할 헤더
```cpp
#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "ExPlayerStateBase.generated.h"
```

### 5.2 UExInventoryComponent — 유연한 부착 설계

| 항목 | 값 |
|---|---|
| 클래스명 | `UExInventoryComponent` |
| 부모 클래스 | `UActorComponent` |
| 배치 위치 | `Plugins/GameFeatures/ExCore/Source/ExCoreRuntime/Components/` |

> **참고:** 현재 러너 게임에서는 인벤토리가 불필요하므로 구조만 구현한다. 실제 아이템 데이터와 로직은 필요 시 확장한다.

#### 5.2.1 부착 대상에 독립적인 설계 원칙

이 컴포넌트는 `PlayerController`, `PlayerState`, `Pawn` 등 어디에 부착되더라도 정상 동작해야 한다.

**핵심 규칙:**
- 컴포넌트 내부에서 `Cast<APlayerController>(GetOwner())` 같은 특정 타입 캐스트를 하드코딩하지 마라.
- 리플리케이션 권한은 `GetOwner()->HasAuthority()`로 판단한다. 이는 부착 대상이 누구든 동일하게 동작한다.
- Server RPC는 `GetOwnerRole()` 기반으로 정상 작동한다. 단, RPC가 동작하려면 `GetOwner()`가 `PlayerController`를 소유하고 있는 Actor이거나, 해당 Actor가 `SetOwner`로 PlayerController와 연결되어 있어야 한다.

**부착 위치에 따른 차이 (구현자가 판단):**

| 부착 위치 | 다른 클라이언트에게 복제 | RPC 자동 라우팅 | 적합한 경우 |
|---|---|---|---|
| `PlayerController` | X (소유자만 존재) | O (자동) | 소유자만 알면 되는 데이터 |
| `PlayerState` | O (모든 클라이언트) | △ (Owner 설정 필요) | 다른 플레이어에게도 보여야 하는 데이터 |
| `Pawn` | O (조건부) | O (Controller 경유) | Pawn과 생명주기를 같이하는 데이터 |

#### 5.2.2 Server RPC 패턴

```cpp
// 클라이언트가 요청 → 서버가 검증 후 처리
UFUNCTION(Server, Reliable, Category="ExInventory")
void Server_UseItem(FName ItemID);

// 서버 구현부
void UExInventoryComponent::Server_UseItem_Implementation(FName ItemID)
{
    // 1. 유효성 검증 (아이템 보유 여부, 사용 가능 상태 등)
    if (!ValidateItemUsage(ItemID)) return;
    
    // 2. 서버 권한으로 데이터 수정
    RemoveItemFromInventory(ItemID);
    ApplyItemEffect(ItemID);
    
    // 3. 수정된 데이터는 Replicated 프로퍼티로 자동 동기화
}
```

#### 5.2.3 인벤토리 배열 복제 최적화 (향후 구현 시 적용)

일반 `TArray<FInventoryItem>`에 `UPROPERTY(Replicated)`를 사용하면 배열 전체가 매번 동기화되어 대역폭이 낭비된다.
향후 실제 인벤토리를 구현할 때는 `FFastArraySerializer`를 활용하여 변경된 요소만 delta로 동기화하라.

```cpp
// 향후 구현 시 참고 구조
USTRUCT()
struct FExInventoryEntry : public FFastArraySerializerItem
{
    GENERATED_BODY()
    
    UPROPERTY()
    FName ItemID;
    
    UPROPERTY()
    int32 StackCount = 0;
    
    void PreReplicatedRemove(const FExInventoryArray& ArraySerializer);
    void PostReplicatedAdd(const FExInventoryArray& ArraySerializer);
    void PostReplicatedChange(const FExInventoryArray& ArraySerializer);
};

USTRUCT()
struct FExInventoryArray : public FFastArraySerializer
{
    GENERATED_BODY()
    
    UPROPERTY()
    TArray<FExInventoryEntry> Items;
    
    bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
    {
        return FFastArraySerializer::FastArrayDeltaSerialize<FExInventoryEntry, FExInventoryArray>(Items, DeltaParms, *this);
    }
};
```

### 5.3 검증 체크리스트
- [ ] `AExPlayerStateBase::AddScore()`가 서버에서만 실행된다. (`BlueprintAuthorityOnly` 확인)
- [ ] `CurrentScore` 변경이 모든 클라이언트에 복제되고 `OnScoreChanged` 델리게이트가 브로드캐스트된다.
- [ ] `UExInventoryComponent`가 `PlayerController`에 부착했을 때 Server RPC가 정상 동작한다.
- [ ] `UExInventoryComponent`가 `PlayerState`에 부착했을 때 컴파일 에러 없이 동작한다.
- [ ] `Server_UseItem`에서 유효성 검증 로직이 존재한다.
- [ ] `GetLifetimeReplicatedProps`에 `CurrentScore`가 등록되어 있다.

---

## 6. 4단계 구현: 외부 통신 (IDP / 웹 API 연동)

> **실행 지시:** 3단계 승인 후 이 단계를 구현하라.

### 6.1 클래스 정보

| 항목 | 값 |
|---|---|
| 클래스명 | `UExBackendCommunicationSubsystem` |
| 부모 클래스 | `UGameInstanceSubsystem` |
| 배치 위치 | `Plugins/GameFeatures/ExCore/Source/ExCoreRuntime/Subsystems/` |

### 6.2 핵심 요구사항

#### 6.2.1 요청 상태 관리
```cpp
UENUM(BlueprintType)
enum class EExRequestState : uint8
{
    Idle,
    Requesting,
    Success,
    Failed
};
```

모든 HTTP 요청은 이 상태를 거쳐야 하며, UI에서 현재 요청 상태를 확인할 수 있어야 한다.

#### 6.2.2 에러 처리 및 재시도

| 항목 | 값 |
|---|---|
| 최대 재시도 횟수 | `UPROPERTY(EditDefaultsOnly)` 로 노출, 기본값 3 |
| 재시도 간격 | 지수 백오프 (1초, 2초, 4초...) |
| 타임아웃 | `UPROPERTY(EditDefaultsOnly)` 로 노출, 기본값 10초 |

#### 6.2.3 델리게이트 — 성공과 실패를 반드시 쌍으로

```cpp
// 로그인 결과 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLoginSuccess, const FString&, PlayerDisplayName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLoginFailed, int32, ErrorCode, const FString&, ErrorMessage);

UPROPERTY(BlueprintAssignable, Category="ExBackend")
FOnLoginSuccess OnLoginSuccess;

UPROPERTY(BlueprintAssignable, Category="ExBackend")
FOnLoginFailed OnLoginFailed;
```

- `GameFlowSubsystem`은 `OnLoginSuccess`를 구독하여 `Flow.Auth.IDP → Flow.Lobby`로 전이한다.
- `GameFlowSubsystem`은 `OnLoginFailed`를 구독하여 `Flow.Auth.IDP`에 머물면서 에러 UI를 표시한다.

#### 6.2.4 토큰 관리 (향후 고려사항)
- 인증 토큰(Access Token)과 갱신 토큰(Refresh Token)을 내부적으로 보관한다.
- 토큰 만료 시 자동 갱신을 시도하는 로직의 위치를 이 서브시스템 내에 확보해 둔다.
- 갱신 실패 시 `OnTokenRefreshFailed` 델리게이트를 브로드캐스트하여 Flow 서브시스템이 재인증 Flow로 전환할 수 있게 한다.

#### 6.2.5 포함해야 할 헤더
```cpp
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Http.h"
#include "ExBackendCommunicationSubsystem.generated.h"
```

### 6.3 검증 체크리스트
- [ ] 로그인 성공 시 `OnLoginSuccess`가 브로드캐스트되고, `GameFlowSubsystem`이 `Flow.Lobby`로 전이한다.
- [ ] 로그인 실패 시 `OnLoginFailed`가 브로드캐스트되고, 상태가 `Flow.Auth.IDP`에 유지된다.
- [ ] HTTP 요청 실패 시 설정된 횟수만큼 지수 백오프로 재시도한다.
- [ ] 재시도 횟수 초과 시 `EExRequestState::Failed`로 전환되고 실패 델리게이트가 브로드캐스트된다.
- [ ] 모든 수치(재시도 횟수, 타임아웃 등)가 `UPROPERTY`로 노출되어 있다.

---

## 7. 전체 시스템 상호작용 다이어그램

```
┌──────────────────────────────────────────────────────────────────┐
│                     GameInstance (영구)                           │
│                                                                  │
│  ┌─────────────────────┐    ┌──────────────────────────────┐    │
│  │ UExGameFlowSubsystem│◄──►│UExBackendCommunicationSubsystem│  │
│  │ (앱 레벨 상태)       │    │ (HTTP, IDP, 토큰)              │  │
│  └────────┬────────────┘    └──────────────────────────────┘    │
│           │ OnRequestTravel                                      │
│           │ (델리게이트)                                          │
└───────────┼──────────────────────────────────────────────────────┘
            │
            ▼
┌──────────────────────────────────────────────────────────────────┐
│                     게임 월드 (맵별 존재)                         │
│                                                                  │
│  ┌──────────────────┐    SetMatchPhase     ┌──────────────────┐ │
│  │AExGameModeBase   │ ──────────────────► │AExGameStateBase  │ │
│  │(서버 전용)        │                      │(모든 클라이언트   │ │
│  │- Travel 수행      │                      │ 에게 복제)        │ │
│  │- 매치 관리        │                      │- CurrentMatchPhase│ │
│  │- 스폰 관리        │                      │- OnRep → 델리게이트│ │
│  └──────────────────┘                      └────────┬─────────┘ │
│                                                      │           │
│                                              UI가 구독│           │
│                                                      ▼           │
│  ┌──────────────────┐                      ┌──────────────────┐ │
│  │AExPlayerStateBase│ ◄── 복제 ──────────  │  HUD / Widget    │ │
│  │- CurrentScore    │                      │  (클라이언트 전용) │ │
│  │- OnRep → 델리게이트│ ──── 구독 ─────────►│                  │ │
│  └──────────────────┘                      └──────────────────┘ │
│                                                                  │
│  ┌──────────────────────────────────────┐                       │
│  │ UExInventoryComponent (어디든 부착)   │                       │
│  │ - Server RPC로 서버에 요청            │                       │
│  │ - 서버가 검증 후 Replicated 데이터 수정│                       │
│  └──────────────────────────────────────┘                       │
└──────────────────────────────────────────────────────────────────┘
```

---

## 8. 파일 생성 순서 요약

### 1단계 (우선 구현)
1. `ExCore/Source/ExCoreRuntime/Tags/ExFlowTags.h` / `.cpp`
2. `ExCore/Source/ExCoreRuntime/Subsystems/ExGameFlowSubsystem.h` / `.cpp`

### 2단계 (1단계 승인 후)
3. `ExCore/Source/ExCoreRuntime/Tags/ExMatchTags.h` / `.cpp`
4. `ExCore/Source/ExCoreRuntime/GameModes/ExGameModeBase.h` / `.cpp`
5. `ExCore/Source/ExCoreRuntime/GameModes/ExGameStateBase.h` / `.cpp`

### 3단계 (2단계 승인 후)
6. `ExCore/Source/ExCoreRuntime/Player/ExPlayerStateBase.h` / `.cpp`
7. `ExCore/Source/ExCoreRuntime/Components/ExInventoryComponent.h` / `.cpp`

### 4단계 (3단계 승인 후)
8. `ExCore/Source/ExCoreRuntime/Subsystems/ExBackendCommunicationSubsystem.h` / `.cpp`
