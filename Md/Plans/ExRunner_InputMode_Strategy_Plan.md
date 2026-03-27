# ExRunner 입력 모드 분리 시스템 (Strategy Pattern) 개발 계획서

러너 게임의 입력 시스템을 **Strategy Pattern**으로 분리하여, 기존 수동(Manual) 모드를 보존하면서 자동 달리기(AutoRun) 3레인 모드 및 입력 비활성화(None) 상태를 유연하게 관리합니다.

---

## User Review Required

> [!IMPORTANT]
> **레인 이동 물리 방식 결정**: 자동 모드의 레인 횡이동을 `AddActorWorldOffset`(직접 위치 보정)으로 처리할지, `ProduceInput`의 횡방향 벡터(Mover 시뮬레이션 통합)로 처리할지 선택이 필요합니다. 현재 계획은 `AddActorWorldOffset` 기반이며, Mover 충돌 처리가 필요하면 `ProduceInput` 방식으로 전환해야 합니다.

> [!IMPORTANT]
> **스프린트(Sprint) 처리 방침**: 자동 달리기(AutoRun) 모드에서 플레이어의 수동 스프린트 버튼 입력을 허용할지, 아니면 자동으로 속도가 제어되도록 시스템을 잠글지 결정이 필요합니다. (기본 방침: 점프/슬라이드처럼 쿨다운 하에 허용 혹은 별도 처리 가능성을 열어둠)

> [!IMPORTANT]
> **레인 인덱스 제한(Clamping) 책임 소재**: 3레인 체제(`-1, 0, 1`)에서 좌우 끝 레인 도달 시 입력을 무시하는 로직을 상위 `MovementComponent`에서 클램핑할지, `Strategy` 객체 내부에서 현재 레인값을 읽고 사전에 차단할지 నిర్ణ합니다. (기본 방침: `MovementComponent`에서 `CurrentLaneIndex`를 갱신할 때 `FMath::Clamp` 적용)

> [!WARNING]
> **레인 이동 미검증 상태**: `MoveLeft/MoveRight + UpdateLanePosition` 레인 시스템은 코드 골격만 존재하고 실제 테스트/검증이 된 적 없습니다. 자동 모드 구현 시 이 부분의 안정화가 선행되어야 합니다.

---

## 서버 권한 / 데디케이티드 서버 검토

### 현재 구조의 네트워크 안전성 분석

| 항목 | 실행 위치 | 네트워크 안전성 | 비고 |
|------|----------|----------------|------|
| `ExInputComponentBase::TickComponent` | **클라이언트 로컬** | ✅ 안전 | `UEnhancedInputLocalPlayerSubsystem`은 로컬 전용. |
| `ExRunnerInputComponent` 델리게이트 브로드캐스트 | **클라이언트 로컬** | ✅ 안전 | Dynamic Multicast는 로컬 이벤트 (RPC 아님). |
| `ExRunnerMovementComponent::ProduceInput` | **클라이언트 로컬** | ✅ 안전 | `IMoverInputProducerInterface`는 Owning Client에서만 실행. Mover가 알아서 서버로 동기화. |
| `ExRunnerMovementComponent::TickComponent` | **클라이언트+서버** | ⚠️ 주의 | `UpdateLanePosition`의 `AddActorWorldOffset`은 Authority/LocallyControlled 체크 필요. |
| Strategy 객체 생성 (UObject) | **클라이언트 로컬** | ✅ 안전 | InputComponent의 Outer로 인스턴스화, 리플리케이션 불필요. |

### 주의점: `UpdateLanePosition`의 `AddActorWorldOffset`
자동 모드에서 레인 횡이동을 `AddActorWorldOffset`으로 처리할 경우, **Owning Client에서만 실행**되도록 로컬 컨트롤 여부 체크 가드가 필요합니다:
```cpp
// 예시 개념: !TargetPawn->IsLocallyControlled() 일 경우 리턴하여 로컬 권한자만 레인 이동 보간 수행
```

---

## 시스템 아키텍처 개요

```
┌──────────────────────────────────────────────────────────────┐
│                    ExGameModeDataSet                         │
│  RunnerInputStrategyClass: TSubclassOf<UExRunnerInputStrategy>│
│  AutoRunLaneChangeSpeed: float                               │
│  AutoRunActionCooldown: float                                │
└──────────────┬───────────────────────────────────────────────┘
               │ BeginPlay 시 클래스 참조 및 인스턴스 생성
               ▼
┌──────────────────────────────────────────────────────────────┐
│              UExRunnerInputComponent                         │
│  ActiveStrategy: TObjectPtr<UExRunnerInputStrategy>          │
│                                                              │
│  NativeOnMoveAction() ──→ ActiveStrategy->HandleHorizontal() │
│  RequestJumpAction()  ──→ ActiveStrategy->CanRequestJump()   │
│  RequestSlideAction() ──→ ActiveStrategy->CanRequestSlide()  │
│  RequestSprintAction()──→ ActiveStrategy->CanRequestSprint() │
│                                                              │
│  ┌─ 공통 델리게이트 분배 (Strategy 내부 판단에 따라) ─────────┐ │
│  │ OnLookRequested            / OnMoveRequested            │ │
│  │ OnJumpRequested            / OnSlideRequested           │ │
│  │ OnSprintRequested          / [NEW] OnLaneChangeRequested│ │
│  └─────────────────────────────────────────────────────────┘ │
└──────────────┬───────────────────────────────────────────────┘
               │ Strategy->BindToMovement()
               ▼
┌──────────────────────────────────────────────────────────────┐
│            UExRunnerMovementComponent                         │
│                                                              │
│  [공통] ProduceInput() ──→ 자동 전진 + GoalDirection         │
│  [공통] UpdateCharacterRotation() ──→ PathManager 경로 추적   │
│  [상태] bIsAutoRunMode (Strategy 바인딩 시 활성화)            │
│                                                              │
│  [Manual] OnLookRequestedCallback ──→ TargetLookYawOffset     │
│  [AutoRun] OnLaneChangeRequestedCallback ──→ MoveLeft/Right   │
│  [AutoRun] UpdateLanePosition() ──→ 횡이동 보간               │
└──────────────────────────────────────────────────────────────┘
```

---

## 1단계: Enum + Strategy 베이스 클래스 및 수명 주기 관리

### [NEW] `ExRunnerInputMode.h` (Enum 클래스)
향후 확장성(시네마틱 컷씬, 대기 상태 등)을 위해 입력 불가 모드를 포함합니다.
- `None` (입력 무시 / 컷씬 등)
- `Manual` (기존 수동 360도 자유 조향)
- `AutoRun` (3레인 스냅 및 쿨다운 기반 액션)

### [NEW] `ExRunnerInputStrategy.h`
`UObject` 기반 클래스. InputComponent는 SetInputMode 호출 시, `NewObject<UExRunnerInputStrategy>(this, ClassType)`를 통해 인스턴스를 생성하고 수명을 관리(`ActiveStrategy`)합니다.

**설계 상세 (개념):**
- 모드별 델리게이트 바인딩 및 해제를 위한 `BindToMovement` / `UnbindFromMovement` 추상화.
- 액션 게이트 검사: `CanRequestJump()`, `CanRequestSlide()`, `CanRequestSprint()` 가상 함수 제공. 기본 값은 `true`(허용).
- 좌우 조향 입력 처리 분기 판별(`HandleHorizontalInput`).

---

## 2단계: Manual Strategy (수동 모드 - 기존 로직 100% 분리)

### 목표
기존 `UExRunnerInputComponent`에 종속되어 있던 수동 입력 로직을 분리시킵니다.
동작 방식의 변질을 방지하기 위해 예외를 엄격히 관리합니다.

**설계 상세 (개념):**
- `HandleHorizontalInput` 발생 시:
  - 잘못된 Look 편입 방지: `NativeOnMoveAction`의 기존 로직 그대로 조이스틱 이동값만 `OnMoveRequested.Broadcast(AxisValue)`에 담아 전송합니다. 
  - 카메라 및 조향 회전값인 `Look`은 스와이프나 다른 입력의 `RequestLookAction` 경로를 온전히 따르도록 냅둡니다.
- 델리게이트 바인딩: `MovementComponent`의 `OnLookRequestedCallback`을 바인딩하여 자유 각도 회전 적용.

---

## 3단계: AutoRun Strategy (자동 달리기 모드 신설)

### 목표
3레인 스냅 이동을 강제하며 연속된 조작 점프/슬라이드 입력을 방어하기 위한 쿨다운 로직을 제공합니다.

**설계 상세 (개념):**
- **입력 스냅 판정 (`HandleHorizontalInput`)**: 
  - 조이스틱 값에서 좌우 X값의 임계치를 검사하여, 한 번의 꺾임당 단 1회의 `OnLaneChangeRequested(-1 또는 +1)` 델리게이트를 발송합니다.
  - 중복 입력 방지 플래그(`bLeftTriggered`, `bRightTriggered`)를 통해 키오프/원복 전까지 다중 발송을 막습니다.
- **액션 쿨다운 (`CanRequestJump` / `CanRequestSlide`)**:
  - `GetWorld()->GetTimeSeconds()`를 사용하여 `LastActionTime`을 기록.
  - `ExGameModeDataSet`의 `AutoRunActionCooldown` (기본 0.3s 등)이 경과하지 않으면 요청 차단(`return false`)
- **델리게이트 바인딩**: 
  - 자유 조향 델리게이트(`OnLookRequested`) 바인딩 해제 (강제 경로 정방향 추적).
  - `OnLaneChangeRequested` 이벤트를 `MovementComponent`의 `MoveLeft` / `MoveRight` 함수와 연결.
  - 바인딩 시점에 MovementComponent 측에 `bIsAutoRunMode = true` 활성화.

---

## 4단계: 기존 컴포넌트 마이그레이션

### `ExRunnerInputComponent` 변경사항
1. **[NEW] 델리게이트 선언**: `FOnRunnerLaneChangeRequested` (레인 이동, +1 / -1) 추가.
2. **Strategy 활성화**: `BeginPlay` 또는 게임 시작 시 `GameModeDataSet`에 설정된 `RunnerInputStrategyClass` 서브클래스를 바탕으로 `ActiveStrategy = NewObject<...>` 생성.
3. **입력 위임 처리**: `NativeOnMoveAction`, `RequestJumpAction` 등 모든 진입점에서 로직을 검사하기 전에 `if(ActiveStrategy->Can...())` 방식으로 1차 게이트 검사 수행. 

### `ExRunnerMovementComponent` 변경사항
1. **[NEW] 모드 플래그**: `bIsAutoRunMode` 불리언 변수 추가. (UpdateLanePosition의 AddActorWorldOffset 보간 연산 활성화 여부에 이용)
2. **레인 인덱스 방어**: `MoveLeft()` / `MoveRight()` 진입 시 `CurrentLaneIndex`를 클램핑 `-1, 0, 1` 안에서만 동작하도록 수정.
3. **Look 바인딩 동적 연결 지원**: `BindLookInput()` 등을 모드 전환마다 안전하게 해제 및 재할당할 수 있도록 구조 개선 (동적으로 AddDynamic/RemoveDynamic 가능하도록 구성).
4. **`ExGameModeDataSet` 종속성**: 컴포넌트 내부에서 `GameModeDataSet`을 참조해 부가 설정값들(`LaneWidth`, `LaneChangeSpeed` 등)을 가져올 수 있도록 처리.
