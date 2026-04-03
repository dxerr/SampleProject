# ExRunner AutoButtonRun 입력 모드 개발 계획서

4방향 버튼(←↑↓→) 기반의 고정 레인 러너 입력 모드를 신설합니다.
기존 `AutoRun` 모드의 드래그/스와이프 조작감 문제를 해결하기 위해, 명확한 이산(Discrete) 버튼 입력으로 전환하여 조작 정확도를 높입니다.

---

## 확정된 기획 및 리뷰 반영 사항

> [!NOTE]
> **1. 좌우 이동 딜레이(쿨타임) 없음**: 좌우 레인 변경 시 `ActionCooldown` 등 입력 딜레이를 적용하지 않고 누르는 즉시 직관적으로 반응하도록 구현합니다.
> **2. 다중 레인 이동은 개별 단위 클릭(Tap)**: 한 번 누르고 있을 때 연속으로 이동하는 홀드 리피트(Hold Repeat) 기능은 사용하지 않습니다. 2개 레인을 이동하려면 유저가 명시적으로 두 번 클릭해야 합니다.
> **3. 점프/슬라이드 체공시간 연동**: 위(점프), 아래(슬라이드) 버튼은 누르고 있는 시간에 비례해 동작 유지 시간이 결정되어야 하므로 기존과 동일하게 `Press -> Release` 방식의 이벤트를 연동합니다.
> **4. HUD 초기화 이슈 보완**: UI WidgetSwitcher 전환 시점이 레벨 로딩 타이밍에 의해 꼬이는 것을 막기 위해 동기화 방어 코드를 추가합니다.
> **5. 네이밍 변경**: 모드 이름을 기존 `AutoRunFix`에서 좀 더 직관적인 `AutoButtonRun`으로 변경합니다.

---

## 시스템 아키텍처 개요

### 기존 Strategy Pattern 확장

```
EExRunnerInputMode
├── None           (입력 비활성화)
├── Manual         (360도 자유 조향)
├── AutoRun        (드래그/스와이프 3레인)
└── [NEW] AutoButtonRun  (4방향 버튼 3레인)
```

`AutoButtonRun` 모드는 **`AutoRun`과 동일한 Movement 로직**(3레인 스냅, 경로 자동 추적, Pure Pursuit 조향)을 공유하며, **입력 수신 방식만 다릅니다**.
따라서 `UExRunnerInputStrategy_AutoRun`을 **상속**하여 최소한의 오버라이드로 구현합니다.

### AutoRun vs AutoButtonRun 차이점

| 항목 | AutoRun (기존) | AutoButtonRun (신규) |
|------|---------------|-------------------|
| **좌우 이동** | 드래그 X축 임계치 판정 | ← → 버튼 단일 클릭 (다중 레인 이동 시 여러 번 탭) |
| **점프** | 위로 스와이프 (Inject Bool) | ↑ 버튼 Down=true / Release=false |
| **슬라이드** | 아래로 스와이프 (Inject Bool) | ↓ 버튼 Down=true / Release=false |
| **스프린트** | 수동 / 미사용 | 미사용 (아이템 버프 전용) |
| **쿨다운** | `AutoRunActionCooldown` 좌우/상하 공유 | 좌우: **쿨다운 없음 (즉시 반응)** / 상하: 유지 |
| **UI** | `WBP_ExTouchPad` | `WBP_ExButtonPad` (신규) |
| **MovementComp** | 동일 | 동일 (부모 클래스 재사용) |

### 아키텍처 다이어그램

```
┌────────────────────────────────────────────────────────┐
│              WBP_ExButtonPad (신규 UMG)                │
│                                                        │
│  ┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐               │
│  │  ←   │  │  →   │  │  ↑   │  │  ↓   │               │
│  │ Left │  │Right │  │Jump  │  │Slide │               │
│  └──┬───┘  └──┬───┘  └──┬───┘  └──┬───┘               │
│     │         │         │         │                    │
│   OnClicked (Left/Right)    OnPressed/OnReleased       │
└─────┼─────────┼─────────┼─────────┼────────────────────┘
      │         │         │         │
      ▼         ▼         ▼         ▼
┌────────────────────────────────────────────────────────┐
│           UExRunnerInputComponent                      │
│                                                        │
│  [NEW] RequestLaneChange(int32 Dir)                    │
│         → ActiveStrategy->HandleLaneChangeRequest(Dir) │
│                                                        │
│  RequestJumpAction(bool bTriggered)                    │
│         → ActiveStrategy->CanRequestJump() 게이트      │
│                                                        │
│  RequestSlideAction(bool bTriggered)                   │
│         → ActiveStrategy->CanRequestSlide() 게이트     │
└────────────────────────────────────────────────────────┘
      │
      ▼
┌────────────────────────────────────────────────────────┐
│     UExRunnerInputStrategy_AutoButtonRun               │
│     (extends AutoRun)                                  │
│                                                        │
│  HandleHorizontalInput() → 비활성화 (드래그 무시)       │
│  HandleLaneChangeRequest(Dir) → OnLaneChangeRequested  │
│     + 보간 완료 & 레인 범위 체크 (부모 로직 재사용)      │
│     ★ (쿨타임 제거 - 누르는 즉시 즉각 Broadcast처리)      │
│  CanRequestJump() → 부모 쿨다운 로직 그대로             │
│  CanRequestSlide() → 부모 쿨다운 로직 그대로            │
└────────────────────────────────────────────────────────┘
```

---

## 네트워크 안전성 분석

| 항목 | 실행 위치 | 안전성 | 비고 |
|------|----------|--------|------|
| `WBP_ExButtonPad` 버튼 이벤트 | 클라이언트 로컬 | ✅ | UMG 위젯은 Local Player 전용 |
| `RequestLaneChange` | 클라이언트 로컬 | ✅ | InputComponent는 로컬 전용 |
| `AutoButtonRun Strategy` 판정 | 클라이언트 로컬 | ✅ | UObject, 리플리케이션 불필요 |
| `OnLaneChangeRequested` Broadcast | 클라이언트 로컬 | ✅ | Dynamic Multicast, RPC 아님 |
| MovementComp 바인딩 | 기존과 동일 | ✅ | 부모 `AutoRun` Strategy 로직 재사용 |

---

## 1단계: Enum 확장 + AutoButtonRun Strategy 클래스

### 1-1. `ExRunnerInputMode.h` 수정

```cpp
UENUM(BlueprintType)
enum class EExRunnerInputMode : uint8
{
    None          UMETA(DisplayName = "없음 (입력 비활성화)"),
    Manual        UMETA(DisplayName = "수동 입력"),
    AutoRun       UMETA(DisplayName = "자동 달리기 (3레인 드래그)"),
    AutoButtonRun UMETA(DisplayName = "자동 달리기 (4방향 버튼)"),  // [NEW]
};
```

### 1-2. `ExRunnerInputStrategy.h` — 베이스 클래스에 가상 함수 추가

```cpp
/**
 * 이산(Discrete) 레인 변경 요청 처리
 * AutoButtonRun: UI 버튼에서 직접 방향값(-1/+1) 전달
 * 기본 구현: HandleHorizontalInput으로 폴백 (Manual/AutoRun 호환)
 */
virtual void HandleLaneChangeRequest(int32 LaneDirection);
```

기본 구현은 아무것도 하지 않거나, 기존 `HandleHorizontalInput`에 위임합니다.
`AutoRun`은 이 함수를 오버라이드하지 않으므로 기존 동작에 영향 없습니다.

### 1-3. [NEW] `ExRunnerInputStrategy_AutoButtonRun.h/.cpp`

**헤더:**
```cpp
UCLASS(DisplayName = "자동 달리기 (4방향 버튼)")
class EXRUNNERPLAYRUNTIME_API UExRunnerInputStrategy_AutoButtonRun 
    : public UExRunnerInputStrategy_AutoRun
{
    GENERATED_BODY()

public:
    /** 드래그 입력 무시 — 이 모드에서는 버튼만 사용 */
    virtual void HandleHorizontalInput(const FVector2D& AxisValue) override;

    /** 버튼 기반 이산 레인 변경 (보간 완료 + 범위 체크 후 쿨다운 없이 Broadcast) */
    virtual void HandleLaneChangeRequest(int32 LaneDirection) override;
};
```

**구현 핵심:**
```cpp
void UExRunnerInputStrategy_AutoButtonRun::HandleHorizontalInput(const FVector2D& AxisValue)
{
    // 드래그/스와이프 입력을 의도적으로 무시
    // AutoButtonRun 모드에서는 버튼 UI가 HandleLaneChangeRequest를 직접 호출
}

void UExRunnerInputStrategy_AutoButtonRun::HandleLaneChangeRequest(int32 LaneDirection)
{
    if (!OwnerInput) return;

    UExRunnerMovementComponent* MovComp = CachedMovementComp.Get();
    if (!MovComp) return;

    // 보간 완료 + 레인 범위 체크 (AutoRun 부모의 HandleHorizontalInput 내부 로직과 동일 기준)
    if (!MovComp->IsLaneTransitionComplete()) return;

    if (LaneDirection > 0 && MovComp->GetCurrentLaneIndex() >= 1) return;
    if (LaneDirection < 0 && MovComp->GetCurrentLaneIndex() <= -1) return;

    // 쿨다운 체크 제거 (기획 확정: 입력 딜레이 없이 즉각 반응)

    OwnerInput->OnLaneChangeRequested.Broadcast(LaneDirection);
}
```

> **설계 결정: `CachedMovementComp` 접근**
> 부모 `AutoRun`에서 `private`으로 선언되어 있으므로, 상속을 위해 부모 측에서 해당 변수를 `protected`로 변경해야 합니다.

---

## 2단계: 기존 컴포넌트 마이그레이션

### 2-1. `ExRunnerInputComponent` 변경사항

**[NEW] `RequestLaneChange` 함수 추가:**
```cpp
/** 이산(Discrete) 레인 변경 요청 — UI 버튼에서 직접 호출 */
UFUNCTION(BlueprintCallable, Category="ExInput|Runner|Actions")
void RequestLaneChange(int32 LaneDirection);
```

구현:
```cpp
void UExRunnerInputComponent::RequestLaneChange(int32 LaneDirection)
{
    if (ActiveStrategy)
    {
        ActiveStrategy->HandleLaneChangeRequest(LaneDirection);
    }
}
```

**`ApplyInputMode` switch 분기 추가:**
```cpp
case EExRunnerInputMode::AutoButtonRun:
    StrategyClass = UExRunnerInputStrategy_AutoButtonRun::StaticClass();
    break;
```

**[NEW] 모드 변경 이벤트 Dispatcher 추가:**
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunnerInputModeChanged, EExRunnerInputMode, NewMode);

UPROPERTY(BlueprintAssignable, Category="ExInput|Runner|Events")
FOnRunnerInputModeChanged OnInputModeChanged;
```

`ApplyInputMode` 마지막에 `OnInputModeChanged.Broadcast(NewMode);` 호출.
→ HUDLayout이 이 이벤트를 구독하여 UI 위젯을 교체합니다.

### 2-2. `ExRunnerInputStrategy_AutoRun` 접근 제한 완화

상속을 위해 다음 멤버를 `private` → `protected`로 변경:
```
- CachedMovementComp  (TWeakObjectPtr)
```
> ※ 상/하(점프/슬라이드)는 자식의 오버라이드 없이 부모 로직을 통해 개별 쿨타임을 공유하므로 `ActionCooldown`, `LastActionTime`은 부모 쪽에 그대로 둬도 무방합니다.

---

## 3단계: UI 위젯 — WBP_ExButtonPad

### 3-1. 위젯 구조 (BP 전용)

```
WBP_ExButtonPad (UserWidget)
├── CanvasPanel (Root)
│
│   ┌─ 좌측 영역 ─────────────────────┐
│   │  Btn_Left  (UButton)            │
│   │    └ Img_ArrowLeft (Image)      │
│   └─────────────────────────────────┘
│
│   ┌─ 우측 영역 ─────────────────────┐
│   │  Btn_Right (UButton)            │
│   │    └ Img_ArrowRight (Image)     │
│   │                                 │
│   │  Btn_Jump  (UButton)            │
│   │    └ Img_ArrowUp (Image)        │
│   │                                 │
│   │  Btn_Slide (UButton)            │
│   │    └ Img_ArrowDown (Image)      │
│   └─────────────────────────────────┘
```

**권장 레이아웃 (가로 모드 기준):**
```
┌──────────────────────────────────────────────┐
│                  게임 화면                     │
│                                              │
│                                              │
│                                              │
│  ┌────┐                          ┌────┐      │
│  │ ←  │                          │ ↑  │      │
│  │Left│                          │Jump│      │
│  └────┘                          └────┘      │
│  ┌────┐                          ┌────┐      │
│  │ →  │                          │ ↓  │      │
│  │Rght│                          │Slid│      │
│  └────┘                          └────┘      │
└──────────────────────────────────────────────┘
  왼손 엄지 영역                  오른손 엄지 영역
```

### 3-2. 버튼 이벤트 → InputComponent 연결 (BP 로직)

**좌/우 버튼 (← →) — 단일 탭 방식:**
- `OnClicked` 이벤트에서 `RequestLaneChange(-1 / +1)`를 1회 호출합니다.
- 홀드 리피트 타이머 관련 매크로나 Blueprint는 추가하지 않습니다. 여러 레인을 가려면 직접 두 번 눌러서 처리합니다.

**점프/슬라이드 버튼 (↑/↓) — Press/Release 방식 달성:**
- `OnPressed` → `RequestJumpAction(true)` / `RequestSlideAction(true)`
- `OnReleased` → `RequestJumpAction(false)` / `RequestSlideAction(false)`
- 누르는 지속 시간에 의해 점프와 슬라이드 스케일/시간이 제어됩니다.

### 3-3. InputComponent 참조 획득 (BP 로직)

위젯 초기화 시:
```
GetOwningPlayerPawn() → FindComponentByClass(UExRunnerInputComponent) → 캐싱
```

---

## 4단계: HUDLayout 위젯 교체 시스템 및 초기화 방어 코드

### 방법: WidgetSwitcher (가장 심플)

`WBP_ExRunnerHUDLayout` 내부에 `UWidgetSwitcher`를 배치하고, 인덱스 0에 `WBP_ExTouchPad`, 인덱스 1에 `WBP_ExButtonPad`를 자식으로 등록합니다.

```
WBP_ExRunnerHUDLayout
├── ... (기존 HUD 요소들)
├── InputPadSwitcher (UWidgetSwitcher)
│   ├── [0] WBP_ExTouchPad        ← Manual / AutoRun
│   └── [1] WBP_ExButtonPad       ← AutoButtonRun
```

**전환 로직 (BP):**
1. **[중요 - 방어 코드]** HUDLayout의 `Construct` 또는 `NativeOnActivated` 시점에 먼저 InputComponent의 포인터를 획득하고, `GetCurrentInputMode`를 통해 현재 모드값을 받아 **WidgetSwitcher의 초기 인덱스를 강제로 세팅**합니다.
2. 이후에 `ExRunnerInputComponent`의 `OnInputModeChanged` 델리게이트를 Bind 처리합니다.
3. 콜백 또는 초기 세팅 함수에서 `NewMode`에 따라 `InputPadSwitcher->SetActiveWidgetIndex(...)`를 호출합니다.
   - `Manual` / `AutoRun` → Index 0 (터치패드)
   - `AutoButtonRun` → Index 1 (버튼패드)
   - `None` → 모두 숨김 (Visibility Collapsed)

---

## 5단계: GameModeDataSet 연동

### 신규 필드 불필요

- 쿨다운: 좌우 레인 변경에는 적용되지 않으며, 상하 점프/슬라이드는 기존대로 `AutoRunActionCooldown` 규칙 내에서 실행가능성을 검토합니다. ✅
- 스프린트: 아이템 버프 전용, Strategy 개입 없음 ✅
- 레인 폭/변경 속도: 기존 MovementComponent 값 공유 ✅

### DefaultInputMode 설정

`ExRunnerInputComponent`의 `DefaultInputMode` 에디터 설정을 `AutoButtonRun`으로 변경하면 게임 시작 시 적용됩니다.

---

## 구현 순서 및 체크리스트

### Phase 1: C++ 인프라 (컴파일 단위)
- [ ] `ExRunnerInputMode.h` — `AutoButtonRun` Enum 값 추가 (기존 `AutoRunFix` 삭제)
- [ ] `ExRunnerInputStrategy_AutoRun.h` — `CachedMovementComp`를 `protected`로 변경
- [ ] `ExRunnerInputStrategy.h/.cpp` — `HandleLaneChangeRequest` 가상 함수 추가
- [ ] `ExRunnerInputStrategy_AutoButtonRun.h/.cpp` — 신규 클래스 생성 (입력 쿨타임 로직 생략, 즉각 이동 처리)
- [ ] `ExRunnerInputComponent.h/.cpp` — `RequestLaneChange` 함수, `OnInputModeChanged` 델리게이트 및 `ApplyInputMode` 분기 처리

### Phase 2: BP/UMG 위젯
- [ ] `WBP_ExButtonPad` 위젯 생성
  - 방향키 버튼 추가
  - 좌/우 버튼은 단일 탭 처리를 위해 `OnClicked`만 바인드
  - 점프/슬라이딩 버튼은 `OnPressed`, `OnReleased` 바인드 (체공 시간 처리 목적)
- [ ] `WBP_ExRunnerHUDLayout`에 WidgetSwitcher 추가 
  - `NativeOnActivated`에 반드시 '최초 1회 Index 동기화 방어 코드' 추가
  - ModeChange 이벤트 수신 로직 연결

### Phase 3: 테스트 및 튜닝
- [ ] PIE에서 모드 적용 후 ← → 버튼 클릭 시, 딜레이 없이 누르는 횟수에 따라 정직하게 레인 이동을 수행하는지 확인
- [ ] 점프/슬라이드 버튼을 길게 누를 때와 짧게 터치할 때, 체공 높이/유형이 의도한 대로 잘 반영되는지 확인
- [ ] 게임 플레이 극초반 로딩/팝업 시 WidgetSwitcher가 누락 없이 올바르게 `WBP_ExButtonPad` 화면을 노출하는지 확인

---

## 제안사항

### 1. 버튼 비주얼 피드백
버튼 Press 시 스케일 축소(0.9배) + 색상 틴트 변경, Release 시 원복하는 간단한 UMG 애니메이션을 추가하면 조작감이 크게 향상됩니다. BP의 `PlayAnimation`으로 간단히 처리 가능합니다.

### 2. 햅틱 피드백
레인 변경 성공 시 `UGameplayStatics::PlayHapticEffect`로 짧은 진동을 주면 터치 기기에서 조작 확인감이 좋아집니다. 기존 `OnLaneChangeRequestedCallback` 내부 또는 BP에서 처리 가능합니다.

### 3. 레인 끝 도달 시 버튼 비활성화 표시
좌측 끝 레인(-1)에서 ← 버튼을 반투명 처리, 우측 끝(+1)에서 → 버튼을 반투명 처리하면 사용자에게 이동 불가 상태를 시각적으로 전달할 수 있습니다. `MovementComponent->GetCurrentLaneIndex()`를 Tick/Timer로 폴링하거나, 레인 변경 완료 이벤트를 추가하여 구현합니다.
