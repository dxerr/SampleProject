# ExRunner 게임 룰 시스템 설계 계획

- **작성일**: 2026-04-10  
- **작성자**: dxerr  
- **상태**: 승인 대기  
- **플러그인**: `ExRunnerPlay` (Feature) — `ExCore`는 수정 없음

---

## 0. 한눈에 보기 (Quick Reference)

> AI/개발자가 이 문서를 처음 읽을 때 전체 범위를 즉시 파악하기 위한 요약.

### 목적
ExRunnerPlay에 **게임 룰(승리/패배 조건) 처리 시스템**을 추가한다.  
낙하 사망, 제한 시간, 목표 거리 달성 등 다양한 룰을 에디터에서 조립할 수 있는 구조.

### 변경 파일 한눈에 보기

| 상태 | 파일 경로 | 역할 |
|:---:|---|---|
| 🆕 신규 | `Rules/ExRunnerRuleBase.h/cpp` | 룰 추상 베이스 (Strategy) |
| 🆕 신규 | `Rules/ExRunnerRule_FallDeath.h/cpp` | 낙하 감지 룰 |
| 🆕 신규 | `Rules/ExRunnerRule_Timer.h/cpp` | 타이머 룰 |
| 🆕 신규 | `Rules/ExRunnerRule_DistanceGoal.h/cpp` | 거리 목표 룰 |
| 🆕 신규 | `Components/ExRunnerRuleManagerComponent.h/cpp` | 룰 관리자 컴포넌트 |
| 🆕 신규 | `Data/ExRunnerRuleConfig.h` | 룰 조합 DataAsset |
| 🆕 신규 | `Struct/EExRunnerGameOverReason.h` | 게임오버 사유 Enum |
| 🆕 신규 | `UI/Widgets/ExRunnerFadeOverlayWidget.h/cpp` | 낙하 페이드아웃 위젯 |
| ✏️ 수정 | `GameStates/ExRunnerGameState.h/cpp` | Replicated 프로퍼티 2개 추가 |
| ✏️ 수정 | `UI/ViewModels/ExRunnerMatchViewModel.h/cpp` | FieldNotify 3개 추가 |
| ✏️ 수정 | `GameModes/ExRunnerGameMode.h/cpp` | RuleManagerComponent 부착 |
| ✏️ 수정 | `Tags/ExRunnerTags.h` | 룰 관련 태그 4개 추가 |

### 추가 GameplayTag

```
Ex.Runner.Rule.FallDeath           낙하 사망 발동
Ex.Runner.Rule.TimeUp              시간 초과 발동
Ex.Runner.Rule.GoalReached         목표 거리 달성 발동
Ex.Runner.Rule.Timer.Warning       타이머 경고 구간 진입
Ex.Runner.Player.DeathVolume       플레이어가 Kill Volume 진입 (FallDeath 룰 내부 트리거)
```

---

## 1. 설계 배경 및 원칙

### 기존 코드베이스 패턴 (이 설계가 따르는 선례)

이 시스템은 기존 **`UExObstacleManager` + `UExObstacleSpawnStrategy`** 패턴과 동일한 구조를 사용한다.

```
[기존 선례]                         [이번 설계]
UExObstacleManager                 UExRunnerRuleManagerComponent
  └─ UExObstacleSpawnStrategy[]      └─ UExRunnerRuleBase[]
       ├─ Strategy_Gap                    ├─ Rule_FallDeath
       ├─ Strategy_Slide                  ├─ Rule_Timer
       └─ Strategy_Climb                  └─ Rule_DistanceGoal
  DataAsset으로 조립               DataAsset(RuleConfig)으로 조립
  GameplayTag로 이벤트 통신        동일
```

### 설계 원칙
- **Strategy Pattern**: 룰 종류마다 독립 클래스, 매니저는 룰 내용을 몰라도 됨
- **서버 권한**: 모든 룰 판정은 `HasAuthority()` 확인 후 처리 (Dedicated Server 기준)
- **Data-Driven**: `DataAsset(UExRunnerRuleConfig)`으로 어떤 룰 조합을 쓸지 에디터에서 조립
- **레이어 분리**: 룰 로직(서버) → GameState Replicated → ViewModel FieldNotify → Widget (클라이언트)

---

## 2. 전체 데이터 흐름

> 이 섹션을 먼저 읽으면 나머지 세부 사항의 맥락을 잡을 수 있다.

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ 서버 ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

AExRunnerGameMode
  └─ UExRunnerRuleManagerComponent
       ├─ Rule_FallDeath  ── Kill Volume Overlap 이벤트 구독 (폴링 없음)
       ├─ Rule_Timer      ── TickRule(): RemainingTime 카운트다운
       └─ Rule_DistanceGoal── TickRule(): 현재 거리 vs 목표 비교
                │
                │ 조건 충족 시 OnRuleTriggered.Broadcast(FGameplayTag)
                ▼
       UExRunnerRuleManagerComponent::OnRuleTriggered()
                │
                ├─ [1] HasAuthority() 확인 + 중복 방지 플래그
                ├─ [2] GameState->SetGameOverReason(Reason)  ── Replicated ──▶
                └─ [3] EventSubsystem->BroadcastTag(Tag) → GameMode::EndMatch()

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ 클라이언트 ━━━━━━━━━━━━━━━━━━━━━━━━━━━

AExRunnerGameState::OnRep_GameOverReason()
  └─ OnGameOverReasonChanged.Broadcast(Reason)
       └─ UExRunnerMatchViewModel::OnGameOverReasonUpdated(Reason)
            │
            ├─ [FallDeath]
            │     UIManagerSubsystem->PushGameOverlay(FadeOverlayWidget)
            │       └─ FadeIn 완료 → ShowConfirm("재시작?")  ← ExPopupWidget 재사용
            │
            └─ [TimeUp / GoalReached]
                  ActiveWidgetIndex = 2 (PostMatch)  ← 기존 매핑 재사용
                    └─ CommonAnimatedSwitcher → 결과 화면 표시
                         └─ 위젯이 GameOverReason FieldNotify로 내용 분기

AExRunnerGameState::OnRep_RemainingTime()  (Timer 룰 전용, 초당 1회)
  └─ OnRemainingTimeChanged.Broadcast(Time)
       └─ UExRunnerMatchViewModel::OnRemainingTimeUpdated(Time)
            ├─ RemainingTime FieldNotify → 타이머 HUD 위젯 갱신
            └─ bIsTimerWarning FieldNotify → 경고 애니메이션 트리거
```

> **UI 레이어 분리 원칙**: GameState는 UI를 직접 호출하지 않는다.  
> UI 결정은 반드시 ViewModel(`OnGameOverReasonUpdated`)에서 이루어진다.

---

## 3. 클래스 인터페이스 상세

### 3.1 `EExRunnerGameOverReason`
```
🆕 Source/ExRunnerPlayRuntime/Struct/EExRunnerGameOverReason.h
```
```cpp
UENUM(BlueprintType)
enum class EExRunnerGameOverReason : uint8
{
    None,
    FallDeath,      // 낙하 사망 → 페이드아웃 + 재시작 팝업
    TimeUp,         // 시간 초과 → 결과 화면
    GoalReached,    // 목표 달성 → 결과 화면 (성공)
};
```

---

### 3.2 `UExRunnerRuleBase` — 추상 베이스
```
🆕 Source/ExRunnerPlayRuntime/Rules/ExRunnerRuleBase.h
```
| 메타 | `UObject`, `Abstract`, `EditInlineNew` |
|---|---|

```cpp
// ── 수명 주기 ──────────────────────────────────────────────────
virtual void InitializeRule(AExRunnerGameMode* InGameMode);   // 참조 주입
virtual void ActivateRule();                                   // 매치 시작 시
virtual void DeactivateRule();                                 // 게임오버/종료 시
virtual void TickRule(float DeltaTime);                        // bTickEnabled=true만 호출

// ── 설정 (에디터 노출) ──────────────────────────────────────────
UPROPERTY(EditAnywhere) FGameplayTag TriggerTag;               // 발동 시 브로드캐스트
UPROPERTY(EditAnywhere) bool bTickEnabled = false;

// ── 발동 알림 (RuleManager가 구독) ─────────────────────────────
FOnRuleTriggered OnRuleTriggered;   // Broadcast(TriggerTag) 호출
```

> ⚠️ **구현 주의**: `UExRunnerRuleBase`는 `UObject` 기반이므로 `GetWorld()` 직접 호출 불가.  
> `InitializeRule(GameMode*)`로 주입받은 참조를 통해 `InGameMode->GetWorld()->GetSubsystem<UExGameplayEventSubsystem>()` 경로로 접근할 것.

---

### 3.3 `UExRunnerRule_FallDeath`
```
🆕 Source/ExRunnerPlayRuntime/Rules/ExRunnerRule_FallDeath.h
```

> ⚠️ **주의**: `AExFloorChunk::KillZ`는 청크가 플레이어 뒤로 멀어질 때 오브젝트 풀로 반환하는 청크 재활용 메커니즘이며, 플레이어 낙하 감지와 무관하다. FallDeath 감지에 사용 불가.

**감지 방식: Kill Volume (UBoxComponent 트리거)**

GameMode가 맵 바닥 아래 일정 Z에 `UBoxComponent` 트리거를 배치하고, 플레이어 캐릭터가 Overlap 시 GameplayTag를 브로드캐스트한다.
- Tick 부하 없음 — 이벤트 구독 방식
- `bTickEnabled = false`

```cpp
UPROPERTY(EditAnywhere) float KillVolumeZ = -1500.f;          // Kill Volume 배치 Z 좌표 (cm)
UPROPERTY(EditAnywhere) FGameplayTag DeathVolumeTag;           // = Ex.Runner.Player.DeathVolume
```

**Kill Volume 설정 흐름**

> ⚠️ `SpawnKillVolume()`은 `AExRunnerGameMode`에 **신규 추가 필요**한 함수이다.  
> Kill Volume Actor는 `RuleManagerComponent`가 소유하며, `DeactivateAllRules()` 시 함께 파괴한다.  
> 매치 재시작 시 중복 생성 방지를 위해 기존 Volume이 있으면 Skip한다.  
> XY 범위는 게임 월드 전체를 커버하도록 충분히 넓게 설정 (예: 20000×20000 cm).

```
ActivateRule()
  ├─ GameMode->SpawnKillVolume(KillVolumeZ)    // UBoxComponent 트리거, XY 20000×20000
  │    └─ 기존 Volume 존재 시 Skip (중복 생성 방지)
  └─ EventSubsystem.Listen(Ex.Runner.Player.DeathVolume, OnPlayerEnteredDeathVolume)

DeactivateRule()
  └─ SpawnedKillVolume->DestroyComponent()     // Volume 정리

OnPlayerEnteredDeathVolume()
  └─ OnRuleTriggered.Broadcast(Ex.Runner.Rule.FallDeath)
```

**추가 GameplayTag** (ExRunnerTags에 추가)
```
Ex.Runner.Player.DeathVolume    플레이어가 Kill Volume에 진입 시 브로드캐스트
```

---

### 3.4 `UExRunnerRule_Timer`
```
🆕 Source/ExRunnerPlayRuntime/Rules/ExRunnerRule_Timer.h
```
- `bTickEnabled = true`

```cpp
UPROPERTY(EditAnywhere) float TotalTime    = 60.f;   // 총 제한 시간 (초)
UPROPERTY(EditAnywhere) float WarningTime  = 10.f;   // 경고 시작 기준 (초)
```

**틱 흐름**
```
TickRule(DeltaTime)
  ├─ ServerRemainingTime -= DeltaTime
  ├─ int32 NewSecond = FMath::FloorToInt(ServerRemainingTime)
  ├─ NewSecond != LastBroadcastSecond                    // 정수 초 변경 시에만 Replicated
  │    ├─ LastBroadcastSecond = NewSecond
  │    └─ GameState->SetRemainingTimeSeconds(NewSecond)  // → OnRep → ViewModel (초당 1회)
  ├─ ServerRemainingTime <= WarningTime (최초 1회)
  │    → EventSubsystem.Broadcast(Ex.Runner.Rule.Timer.Warning)
  └─ ServerRemainingTime <= 0
       → OnRuleTriggered.Broadcast(Ex.Runner.Rule.TimeUp)
```

> **대역폭 최적화**: 서버는 초당 1회만 Replicated. 클라이언트는 수신한 정수 초를 기준으로 로컬에서 소수점 카운트다운을 보간하여 HUD를 부드럽게 갱신한다.

---

### 3.5 `UExRunnerRule_DistanceGoal`
```
🆕 Source/ExRunnerPlayRuntime/Rules/ExRunnerRule_DistanceGoal.h
```
- `bTickEnabled = true`

```cpp
UPROPERTY(EditAnywhere) float GoalDistance = 10000.f;   // 달성 목표 거리 (cm)
```

**틱 흐름**
```
TickRule()
  └─ GameState->CurrentPathDistance >= GoalDistance
       → OnRuleTriggered.Broadcast(Ex.Runner.Rule.GoalReached)
```

---

### 3.6 `UExRunnerRuleManagerComponent`
```
🆕 Source/ExRunnerPlayRuntime/Components/ExRunnerRuleManagerComponent.h
```
`AExRunnerGameMode`에 컴포넌트로 부착. `BeginPlay`에서 GameMode가 `ActivateAllRules()` 호출.

```cpp
// ── 설정 ──────────────────────────────────────────────────────
UPROPERTY(EditAnywhere)                        // Instanced 아님 — DataAsset 외부 참조
TObjectPtr<UExRunnerRuleConfig> RuleConfig;

// ── 공개 인터페이스 ────────────────────────────────────────────
void ActivateAllRules();    // 매치 시작 시 GameMode가 호출
void DeactivateAllRules();  // EndMatch 시 GameMode가 호출

// ── 내부 룰 발동 수신 ──────────────────────────────────────────
void OnRuleTriggered(FGameplayTag ResultTag)
{
    if (!GetOwner()->HasAuthority()) return;
    if (bGameOverHandled) return;   // 복수 룰 동시 발동 방지
    bGameOverHandled = true;

    GameState->SetGameOverReason(TagToReason(ResultTag));        // Replicated
    EventSubsystem->BroadcastTag(ResultTag);                     // GameMode::EndMatch
}
```

---

### 3.7 `UExRunnerRuleConfig` — DataAsset
```
🆕 Source/ExRunnerPlayRuntime/Data/ExRunnerRuleConfig.h
```
```cpp
// UDataAsset 상속 필수 — Content Browser에서 DA_ExRule_* 에셋으로 독립 생성 가능
UCLASS()
class UExRunnerRuleConfig : public UDataAsset
{
    GENERATED_BODY()
public:
    // DataAsset 내부에서 룰 인스턴스를 인라인으로 조립 (EditInlineNew)
    UPROPERTY(EditDefaultsOnly, Instanced)
    TArray<TObjectPtr<UExRunnerRuleBase>> Rules;
};
```

> **설계 결정**: `UDataAsset` 상속으로 Content Browser에서 독립 에셋(`DA_ExRule_*`)을 생성하고, `RuleManagerComponent`는 해당 에셋을 외부 참조(`EditAnywhere`, Instanced 아님)한다. 룰 인스턴스 인라인 편집은 DataAsset 내부에서 이루어진다.

| 에셋 예시 | 구성 룰 |
|---|---|
| `DA_ExRule_TimerMode` | Timer + FallDeath |
| `DA_ExRule_EndlessMode` | FallDeath only |
| `DA_ExRule_DistanceMode` | DistanceGoal + FallDeath |

---

## 4. 기존 파일 수정 사항

### 4.1 `AExRunnerGameState` — Replicated 항목 추가
```
✏️ Source/ExRunnerPlayRuntime/GameStates/ExRunnerGameState.h/cpp
```
```cpp
// Timer 룰이 초당 1회 갱신 → 클라이언트 HUD 타이머 표시 (매 프레임 Replicated 방지)
UPROPERTY(ReplicatedUsing = OnRep_RemainingTime)
int32 RemainingTimeSeconds = 0;   // 정수 초 단위 — 변경 시에만 전송

// RuleManager가 게임오버 시 설정 → 클라이언트 UI 분기
UPROPERTY(ReplicatedUsing = OnRep_GameOverReason)
EExRunnerGameOverReason GameOverReason = EExRunnerGameOverReason::None;

// ViewModel이 구독하는 변경 알림 델리게이트
FOnRemainingTimeChanged   OnRemainingTimeChanged;    // int32 초 전달
FOnGameOverReasonChanged  OnGameOverReasonChanged;
```
> OnRep 콜백은 해당 델리게이트를 Broadcast하는 것 외에 UI 로직을 포함하지 않는다.

---

### 4.2 `UExRunnerMatchViewModel` — FieldNotify 추가
```
✏️ Source/ExRunnerPlayRuntime/UI/ViewModels/ExRunnerMatchViewModel.h/cpp
```

**추가 FieldNotify**

| 프로퍼티 | 타입 | 구독 소스 | 용도 |
|---|---|---|---|
| `RemainingTime` | `float` | `GameState->OnRemainingTimeChanged` | 타이머 HUD 위젯 갱신 (로컬 보간으로 부드럽게) |
| `bIsTimerWarning` | `bool` | 위와 동일 (파생 계산) | 경고 구간 깜빡임 애니 |
| `GameOverReason` | `EExRunnerGameOverReason` | `GameState->OnGameOverReasonChanged` | 결과 화면 내용 분기 |

**`AutoInitialize()` 추가 바인딩**
```cpp
GameState->OnRemainingTimeChanged.AddDynamic(this, &OnRemainingTimeUpdated);   // int32 초 수신
GameState->OnGameOverReasonChanged.AddDynamic(this, &OnGameOverReasonUpdated);
```

**`OnGameOverReasonUpdated()` — UI 분기 결정 지점**
```cpp
void UExRunnerMatchViewModel::OnGameOverReasonUpdated(EExRunnerGameOverReason Reason)
{
    UE_MVVM_SET_PROPERTY_VALUE(GameOverReason, Reason);

    if (Reason == EExRunnerGameOverReason::FallDeath)
    {
        // 페이드아웃 오버레이 → 재시작 팝업 흐름
        UIManagerSubsystem->PushGameOverlay(ExRunnerFadeOverlayWidgetClass);
    }
    else if (Reason != EExRunnerGameOverReason::None)
    {
        // 결과 화면으로 전환 (기존 PostMatch 매핑 재사용)
        SetActiveWidgetIndex(2);
    }
}
```
> **핵심**: UI 경로 결정은 이 함수 한 곳에서만 이루어진다. GameState·RuleManager는 UI를 모른다.

---

### 4.3 `AExRunnerGameMode` — RuleManagerComponent 부착
```
✏️ Source/ExRunnerPlayRuntime/GameModes/ExRunnerGameMode.h/cpp
```
```cpp
UPROPERTY(VisibleAnywhere)
TObjectPtr<UExRunnerRuleManagerComponent> RuleManagerComponent;   // CreateDefaultSubobject

// 매치 시작 시
void OnMatchStarted_Implementation() override
{
    Super::OnMatchStarted_Implementation();
    RuleManagerComponent->ActivateAllRules();
}

// 매치 종료 시
void OnMatchEnded_Implementation() override
{
    RuleManagerComponent->DeactivateAllRules();
    Super::OnMatchEnded_Implementation();
}
```

---

## 5. 신규 UI 위젯

### `UExRunnerFadeOverlayWidget` — 낙하 페이드아웃 전용
```
🆕 Source/ExRunnerPlayRuntime/UI/Widgets/ExRunnerFadeOverlayWidget.h/cpp
```
- 부모: `UCommonActivatableWidget`
- 배치: `GameStack` (게임 오버레이 레이어 — 입력 차단 없음)

```cpp
// ViewModel이 PushGameOverlay() 후 즉시 호출
UFUNCTION(BlueprintCallable)
void PlayFadeIn(float Duration);

// 페이드 완료 시 BP에서 호출 → 재시작 팝업 표시
UFUNCTION(BlueprintNativeEvent)
void OnFadeComplete();

// 기본 구현: ExPopupWidget(Confirm) 표시
void OnFadeComplete_Implementation()
{
    // FOnExPopupResultNative — ExUIManagerSubsystem::ShowConfirm의 실제 콜백 타입
    UIManagerSubsystem->ShowConfirm(
        LOCTEXT("GameOver", "게임 오버"),
        LOCTEXT("RestartPrompt", "다시 시작하시겠습니까?"),
        FOnExPopupResultNative::CreateUObject(this, &UExRunnerFadeOverlayWidget::HandleRestartResult)
    );
}

// 팝업 결과 콜백 — DECLARE_DELEGATE_TwoParams(FOnExPopupResultNative, EExModalResult, const FText&)
void HandleRestartResult(EExModalResult Result, const FText& InputText);
```

**FallDeath 전체 UI 시퀀스**
```
MatchViewModel::OnGameOverReasonUpdated(FallDeath)
  └─ UIManagerSubsystem->PushGameOverlay(FadeOverlayWidget)
       └─ PlayFadeIn(1.5f)           ← 화면 서서히 어두워짐
            └─ OnFadeComplete()
                 └─ ShowConfirm()    ← ExPopupWidget 재사용
                      ├─ Confirm → GameMode::RequestRestartMatch()
                      └─ Cancel  → GameMode::ReturnToLobby()
```

---

## 6. 기존 코드 재사용 목록

| 재사용 대상 | 활용 방식 |
|---|---|
| `UExUIManagerSubsystem::ShowConfirm()` | 재시작 팝업 — 코드 변경 없음 |
| `UExPopupWidget` | 팝업 UI — 코드 변경 없음 |
| `ExRunnerMatchViewModel::ActiveWidgetIndex` (index 2) | TimeUp/GoalReached 결과 화면 전환 — 코드 변경 없음 |
| `UExGameplayEventSubsystem` | 룰 이벤트 전파 — 코드 변경 없음 |
| `ExRunnerTags` 네임스페이스 | 태그 5개 추가 (`Ex.Runner.Player.DeathVolume` 포함) |

---

## 7. 구현 순서

의존성 순서로 정렬. 상위 항목이 완료되어야 하위 항목 구현 가능.

```
1. EExRunnerGameOverReason            (의존성 없음)
2. ExRunnerRuleBase                   (의존성 없음)
3. ExRunnerGameState 수정             (EExRunnerGameOverReason 필요)
4. ExRunnerRuleManagerComponent       (RuleBase, GameState 필요)
5. ExRunnerRuleConfig                 (RuleBase 필요)
6. Rule_FallDeath                     (RuleBase, EventSubsystem 필요)
7. Rule_Timer                         (RuleBase, GameState 필요)
8. Rule_DistanceGoal                  (RuleBase, GameState 필요)
9. ExRunnerMatchViewModel 수정        (GameState 수정 필요)
10. ExRunnerFadeOverlayWidget         (UIManagerSubsystem, PopupWidget 필요)
11. ExRunnerGameMode 수정             (RuleManagerComponent 필요)
12. DataAsset 에셋 생성 + BP 설정     (전체 완료 후)
```

---

## 8. 구현 고도화 예정 항목

> ⚠️ 아래 항목들은 현재 1차 구현에서 누락되었거나 스텁(stub) 상태로, 추후 구현이 필요합니다.

---

### 8.1 `AExGameModeBase::OnMatchEnded_Implementation` 미구현

현재 `ExGameModeBase.cpp`에서 `OnMatchEnded_Implementation()`은 **완전히 빈 함수**입니다.
`AExRunnerGameMode::OnMatchEnded_Implementation()`에서 `Super::OnMatchEnded_Implementation()`을 호출하지만 아무 동작도 없습니다.

**고도화 필요 내용**:
```cpp
// ExGameModeBase.cpp — 향후 구현 예정
void AExGameModeBase::OnMatchEnded_Implementation()
{
    // [ ] 모든 플레이어 입력 비활성화
    // [ ] BGM/SFX 페이드아웃
    // [ ] 타이머 / Tick 일시 정지 (선택)
    // [ ] 결과 데이터 저장 (통계, 점수 등)
}
```

---

### 8.2 `OnRuleTriggered` 이후 캐릭터 상태 처리

룰 발동 시 `GameState::GameOverReason`이 설정되지만, **캐릭터는 아직 물리 시뮬레이션이 그대로 진행**됩니다.

| 문제 | 증상 | 필요 처리 |
|---|---|---|
| **FallDeath** | 낙하 판정 후에도 캐릭터가 계속 떨어짐 | Kill Volume Overlap 즉시 이동 입력 차단, Ragdoll 또는 Death 애니메이션 재생 |
| **TimeUp / GoalReached** | 결과 화면 전환 중에도 캐릭터가 계속 달림 | `bRunnerModeEnabled = false` 또는 입력 비활성화 |

**구현 방향** — `RuleManagerComponent::OnRuleTriggered()` 내에서 처리:
```cpp
// [TODO] 추가 예정
// 1. 플레이어 폰의 Movement 입력 잠금
PlayerController->SetIgnoreMoveInput(true);

// 2. FallDeath: 지정 Anim Montage(Death) 재생 또는 Ragdoll 전환
// 3. TimeUp/GoalReached: 승리/패배 Montage 재생 후 정지 포즈 유지
```

---

### 8.3 UI 고도화 예정

| 항목 | 현재 상태 | 고도화 내용 |
|---|---|---|
| **WBP_FadeOverlay** | C++ 클래스만 존재 | BP 위젯 제작 — 페이드인 UMG 애니메이션, BindWidget, OnFadeComplete 호출 연결 |
| **결과 화면 (index=2)** | 스위처 전환만 됨 | GameOverReason FieldNotify로 "승리/패배/시간초과" 텍스트/연출 분기 |
| **FadeOverlayWidgetClass 연결** | MatchViewModel UPROPERTY 추가됨 | GameMode BP Details에서 WBP_FadeOverlay 에셋 연결 필요 |
| **타이머 HUD** | RemainingTime FieldNotify 브로드캐스트됨 | HUD 위젯에 `GetRemainingTime` 바인딩 + 경고 애니메이션 트리거 (`bIsTimerWarning`) |

---

### 8.4 재시작/로비 복귀 흐름 완성

현재 `UExRunnerFadeOverlayWidget::HandleRestartResult()`에서:

```cpp
// Confirm → RestartLevel() ✅ (임시 구현)
// Cancel  → TODO: GameMode::ReturnToLobby() ❌ 미구현
```

**고도화 필요**:
- `Cancel` 선택 시 `UExGameFlowSubsystem`을 통해 로비 맵으로 ServerTravel
- 멀티플레이 환경에서 Host/Client 각각 처리 분기

