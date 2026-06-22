# ExRunner 입력 모드 시스템 개발 계획서 (통합)

> **버전:** v2.0 (통합판)  
> **작성일:** 2026-04-22  
> **이전 문서:** `ExRunner_InputMode_Strategy_Plan.md` + `ExRunner_AutoButtonRun_InputMode_Plan.md`를 통합  
> **관련 아키텍처:** `ExFrameWork_Input_System_Architecture.md` (ExCore 입력 통합 아키텍처)

---

## 1. 개요

러너 게임의 입력 시스템을 **Strategy Pattern**으로 분리하여, 모드별 입력 처리를 독립적으로 관리한다.

### 1.1 입력 모드 정의

```
EExRunnerInputMode
├── None           (입력 비활성화 / 컷씬 등)
├── Manual         (360도 자유 조향 — 기존 수동 모드)
├── AutoRun        (3레인 드래그/스와이프)
└── AutoButtonRun  (4방향 버튼 3레인 — 터치 조작 최적화)
```

### 1.2 모드별 비교

| 항목 | Manual | AutoRun | AutoButtonRun |
|------|--------|---------|---------------|
| **좌우 이동** | 조이스틱 자유 조향 | 드래그 X축 임계치 판정 | ← → 버튼 단일 탭 |
| **점프** | 키보드/버튼 | 위로 스와이프 | ↑ 버튼 Press/Release |
| **슬라이드** | 키보드/버튼 | 아래로 스와이프 | ↓ 버튼 Press/Release |
| **좌우 쿨다운** | 없음 | `AutoRunActionCooldown` 공유 | **없음 (즉시 반응)** |
| **카메라** | 자유 Look | 경로 자동 추적 | 경로 자동 추적 |
| **UI** | `WBP_ExTouchPad` | `WBP_ExTouchPad` | `WBP_ExButtonPad` |
| **MovementComp** | ProduceInput 전진 + Look Yaw | ProduceInput 전진 + 레인 스냅 | 동일 (AutoRun 재사용) |

---

## 2. 시스템 아키텍처

```
┌──────────────────────────────────────────────────────────────┐
│              UExRunnerInputComponent                         │
│  ActiveStrategy: TObjectPtr<UExRunnerInputStrategy>          │
│                                                              │
│  NativeOnMoveAction() ──→ ActiveStrategy->HandleHorizontal() │
│  RequestJumpAction()  ──→ ActiveStrategy->CanRequestJump()   │
│  RequestSlideAction() ──→ ActiveStrategy->CanRequestSlide()  │
│  RequestLaneChange()  ──→ ActiveStrategy->HandleLaneChange() │
│                                                              │
│  ┌─ 공통 델리게이트 ──────────────────────────────────────────┐ │
│  │ OnJumpRequested / OnSlideRequested / OnSprintRequested   │ │
│  │ OnMoveRequested / OnLookRequested / OnLaneChangeRequested│ │
│  │ OnInputModeChanged                                      │ │
│  └─────────────────────────────────────────────────────────┘ │
└──────────────┬───────────────────────────────────────────────┘
               │ Strategy->BindToMovement()
               ▼
┌──────────────────────────────────────────────────────────────┐
│            UExRunnerMovementComponent                         │
│  [공통] ProduceInput() ──→ 자동 전진 + GoalDirection         │
│  [Manual] OnLookRequestedCallback ──→ TargetLookYawOffset     │
│  [AutoRun/AutoButtonRun] OnLaneChangeRequestedCallback       │
│         ──→ MoveLeft/Right + UpdateLanePosition 보간          │
└──────────────────────────────────────────────────────────────┘
```

### Strategy 클래스 계층

```
UExRunnerInputStrategy (Abstract Base)
├── UExRunnerInputStrategy_Manual       (360도 자유 조향)
├── UExRunnerInputStrategy_AutoRun      (드래그/스와이프 3레인)
│   └── UExRunnerInputStrategy_AutoButtonRun  (4방향 버튼, AutoRun 상속)
└── (None 모드는 Strategy=nullptr로 처리)
```

---

## 3. 네트워크 안전성 분석

| 항목 | 실행 위치 | 안전성 | 비고 |
|------|----------|--------|------|
| Enhanced Input 콜백 | 클라이언트 로컬 | ✅ | `UEnhancedInputLocalPlayerSubsystem`은 로컬 전용 |
| 델리게이트 브로드캐스트 | 클라이언트 로컬 | ✅ | Dynamic Multicast는 로컬 이벤트 (RPC 아님) |
| `ProduceInput` | 클라이언트 로컬 | ✅ | Mover가 서버 동기화 처리 |
| Strategy UObject 생성 | 클라이언트 로컬 | ✅ | InputComponent의 Outer, 리플리케이션 불필요 |
| `UpdateLanePosition` | 클라이언트+서버 | ⚠️ | `AddActorWorldOffset` 사용 시 `IsLocallyControlled()` 체크 필요 |
| UI 버튼 이벤트 | 클라이언트 로컬 | ✅ | UMG 위젯은 Local Player 전용 |

---

## 4. 구현 상세

### 4.1 Strategy 베이스 클래스

`UObject` 기반. InputComponent가 `SetInputMode()` 호출 시 `NewObject`로 인스턴스 생성.

핵심 가상 함수:
- `BindToMovement()` / `UnbindFromMovement()` — 모드별 델리게이트 연결/해제
- `HandleHorizontalInput(FVector2D)` — 좌우 조향 입력 처리 분기
- `HandleLaneChangeRequest(int32 Dir)` — 이산 레인 변경 (AutoButtonRun 전용)
- `CanRequestJump()` / `CanRequestSlide()` / `CanRequestSprint()` — 액션 게이트

### 4.2 Manual Strategy

기존 수동 입력 로직을 100% 분리:
- `HandleHorizontalInput` → `OnMoveRequested.Broadcast(AxisValue)`
- Look(카메라 회전)은 `RequestLookAction` 경로를 통해 처리
- 모든 액션 게이트 기본 `true` (제한 없음)

### 4.3 AutoRun Strategy

3레인 스냅 + 쿨다운 기반:
- `HandleHorizontalInput` → X축 임계치 판정 → `OnLaneChangeRequested.Broadcast(+1/-1)`
- 중복 입력 방지 플래그(`bLeftTriggered`, `bRightTriggered`)
- 액션 쿨다운: `GetWorld()->GetTimeSeconds()` 기반, `AutoRunActionCooldown` 경과 체크
- 자유 조향 델리게이트 해제, 경로 자동 추적 강제

### 4.4 AutoButtonRun Strategy (AutoRun 상속)

AutoRun과 동일한 Movement 로직을 공유하되, 입력 수신 방식만 변경:
- `HandleHorizontalInput()` → **비활성화** (드래그 무시)
- `HandleLaneChangeRequest(int32 Dir)` → 보간 완료 + 레인 범위 체크 후 **쿨다운 없이** 즉시 Broadcast
- 점프/슬라이드는 부모 쿨다운 로직 재사용

```cpp
void UExRunnerInputStrategy_AutoButtonRun::HandleLaneChangeRequest(int32 LaneDirection)
{
    if (!OwnerInput) return;
    UExRunnerMovementComponent* MovComp = CachedMovementComp.Get();
    if (!MovComp) return;

    if (!MovComp->IsLaneTransitionComplete()) return;
    if (LaneDirection > 0 && MovComp->GetCurrentLaneIndex() >= 1) return;
    if (LaneDirection < 0 && MovComp->GetCurrentLaneIndex() <= -1) return;

    // 쿨다운 없음 — 즉시 반응 (기획 확정)
    OwnerInput->OnLaneChangeRequested.Broadcast(LaneDirection);
}
```

> **접근 제한:** 부모 `AutoRun`의 `CachedMovementComp`를 `protected`로 변경 필요.

---

## 5. UI 위젯 — WBP_ExButtonPad

### 5.1 위젯 구조

```
WBP_ExButtonPad (UserWidget)
├── 좌측: Btn_Left (← 단일 탭 OnClicked)
├── 좌측: Btn_Right (→ 단일 탭 OnClicked)  
├── 우측: Btn_Jump (↑ OnPressed/OnReleased → 체공 시간 제어)
└── 우측: Btn_Slide (↓ OnPressed/OnReleased → 슬라이드 유지 제어)
```

### 5.2 HUD 교체 — WidgetSwitcher 방식

```
WBP_ExRunnerHUDLayout
├── InputPadSwitcher (UWidgetSwitcher)
│   ├── [0] WBP_ExTouchPad        ← Manual / AutoRun
│   └── [1] WBP_ExButtonPad       ← AutoButtonRun
```

- `NativeOnActivated`에서 `GetCurrentInputMode()`로 초기 인덱스 강제 설정 (방어 코드)
- `OnInputModeChanged` 델리게이트 구독하여 런타임 전환

---

## 6. 확정된 기획 사항

1. **좌우 이동 딜레이 없음**: AutoButtonRun에서 좌우 레인 변경 시 쿨다운 미적용, 즉시 반응
2. **다중 레인 이동은 개별 탭**: 홀드 리피트 없음. 2레인 이동 시 두 번 클릭
3. **점프/슬라이드 체공 연동**: ↑↓ 버튼의 Press/Release 시간에 비례한 동작 유지
4. **스프린트**: 아이템 버프 전용, Strategy 개입 없음

---

## 7. 미결 사항 (User Review Required)

> [!WARNING]
> **레인 이동 미검증 상태**: `MoveLeft/MoveRight + UpdateLanePosition` 레인 시스템은 코드 골격만 존재하고 실제 테스트/검증이 안 됨. AutoRun/AutoButtonRun 구현 시 안정화 선행 필요.

> [!IMPORTANT]  
> **레인 이동 물리 방식**: 현재 `AddActorWorldOffset` 기반 설계. Mover 충돌 처리가 필요하면 `ProduceInput` 횡방향 벡터 방식으로 전환 검토.

---

## 8. 구현 체크리스트

### Phase 1: C++ 인프라
- [ ] `ExRunnerInputMode.h` — `AutoButtonRun` Enum 값 추가
- [ ] `ExRunnerInputStrategy.h` — `HandleLaneChangeRequest` 가상 함수 추가
- [ ] `ExRunnerInputStrategy_AutoRun.h` — `CachedMovementComp`를 `protected`로 변경
- [ ] `ExRunnerInputStrategy_AutoButtonRun.h/.cpp` — 신규 클래스 (드래그 무시, 즉시 레인 이동)
- [ ] `ExRunnerInputComponent.h/.cpp` — `RequestLaneChange`, `OnInputModeChanged`, `ApplyInputMode` 분기

### Phase 2: BP/UMG
- [ ] `WBP_ExButtonPad` 위젯 생성 (좌/우 OnClicked, 상/하 OnPressed+OnReleased)
- [ ] `WBP_ExRunnerHUDLayout`에 WidgetSwitcher + 초기화 방어 코드 추가

### Phase 3: 테스트
- [ ] ← → 버튼 딜레이 없이 탭 횟수에 따라 정직하게 레인 이동
- [ ] ↑↓ 버튼 길게/짧게 눌렀을 때 체공/슬라이드 시간 반영
- [ ] 극초반 로딩 시 WidgetSwitcher 올바른 인덱스 표시

---

## 9. 터치패드 입력(AutoRun 모드) 참고

AutoRun 모드의 드래그/스와이프 처리는 `ExBaseTouchPadWidget` + `ExRunnerInputViewModel`로 구현되어 있다.

| 축 | 입력 방식 | 처리 |
|---|---|---|
| X축 | `NormalizedOffset.X` (패드 중앙 기준 -1.0~1.0) | `RequestLookAction()` → Yaw 회전 |
| Y축 위 | RelativeY ≤ -SwipeThreshold | `RequestJumpAction(true)` Hold 방식 |
| Y축 아래 | RelativeY ≥ +SwipeThreshold | `RequestSlideAction(true)` Active/Restore 방식 |

> 관련 문서: `Legacy/Mobile_Joystick_Input_Proposal.md`, `Guides/ExRunnerPlay/ExRunner_MobileJoystick_Setup_Guide.md`, `Legacy/MobileJoystick_Work_Handover.md`

---

## 변경 이력

| 날짜 | 버전 | 내용 |
|------|------|------|
| 2026-04-22 | v2.0 | `ExRunner_InputMode_Strategy_Plan.md`와 `ExRunner_AutoButtonRun_InputMode_Plan.md`를 통합 |
