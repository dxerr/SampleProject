# WBP_ExRunnerSpeedBar MVVM 바인딩 및 속도값 갱신 버그

- **날짜**: 2026-03-05 ~ 2026-03-08
- **상태**: ✅ 해결 완료
- **키워드**: `WBP_ExRunnerSpeedBar`, `MVVM`, `FieldNotify`, `ViewBinding`, `ExRunnerStatsViewModel`, `ExRunnerStatComponent`, `MoverComponent`, `GetVelocity`, `AutoInitialize`

---

## 증상

1. `WBP_ExRunnerSpeedBar` 위젯이 화면에 아예 출력되지 않음 (초기)
2. MVVM View Bindings 설정 중 `"not writable at runtime"` 에러 발생
3. 속도값이 UI에 표시는 되나 `0`으로 고정되어 갱신되지 않음

---

## 원인 분석 요약

### 문제 1: View Bindings "not writable at runtime"
- **원인**: `TextBlock_0` 위젯 레퍼런스 자체를 바인딩 Target으로 지정. UTextBlock은 `CPF_BlueprintReadOnly` 플래그를 가져 런타임에서 쓰기 불가
- **해결**: View Bindings에서 Target을 `TextBlock_0.Text` 속성으로 올바르게 지정

### 문제 2: ViewModel 변수가 View Bindings Source/Target에 노출 안 됨
- **원인**: `UFUNCTION`으로 노출한 값은 View Bindings의 Target이 될 수 없음
- **해결**: `UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Getter)`로 전환하여 직접 노출

### 문제 3: float↔FText 타입 불일치
- **원인**: `CurrentSpeed(float)` → `Text(FText)` 직접 바인딩 불가
- **해결**: Source를 `GetCurrentSpeedText()` UFUNCTION(BlueprintPure, FieldNotify)으로 변경 (FText 반환)

### 문제 4: 속도값이 0으로 고정 (UI 미갱신)
- **원인 A**: `InitializeRunnerBindings`가 호출되지 않아 ViewModel이 StatComponent를 구독하지 않음
  - `On Initialized`는 ViewModel 생성 전에 호출됨 → `On Activated`로 변경
- **원인 B**: `StatComponent`가 SkeletalMesh Actor에 부착되어 있어 `Get Owning Player Pawn → Get Components By Class`로 탐색 불가
  - **해결**: `AutoInitialize(PlayerController)` 유틸 함수 추가 (Pawn → AttachedActors 자동 순회)
- **원인 C**: `APawn::GetVelocity()`는 Mover 시스템에서 항상 `0` 반환
  - **해결**: `UMoverComponent::GetVelocity()` 직접 사용

### 문제 5: Tick 기반 속도 업데이트 비효율
- **원인**: `ExRunnerMovementComponent` Tick에서 매 프레임마다 StatComponent에 속도를 밀어넣는 구조
- **해결**: `ExRunnerStatComponent`가 `StatPollInterval(0.1초)` 타이머로 자체 폴링

### 문제 6: Core → Runner 역참조
- **원인**: `ExSandboxCharacter_Mover`(ExCore BP)에 `ExRunnerStatComponent`(ExRunnerPlay) 직접 부착
- **해결**: `AutoInitialize(PlayerController)` 함수로 BP 노드 1개로 완전 초기화, 역참조 제거

---

## 최종 구조

```
[ExRunnerStatComponent] (StatPollInterval 타이머, 0.1초 주기)
  BeginPlay: UExActorUtil::FindOwnerPawn(this) → 상위 Pawn 탐색
             UMoverComponent::GetVelocity()로 속도 수집
  UpdateStats: SetCurrentRunningSpeed() → OnRunnerSpeedChanged.Broadcast()

[ExRunnerStatsViewModel]
  OnSpeedUpdated: SetCurrentSpeed() → UE_MVVM_SET_PROPERTY_VALUE
                → UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetCurrentSpeedText)

[WBP_ExRunnerSpeedBar] View Bindings
  Source: ExRunnerStatsViewModel.GetCurrentSpeedText (FText)
  Target: TextBlock_0.Text

[WBP_ExRunnerSpeedBar] Event Graph
  On Activated → Get ViewModel → AutoInitialize(GetOwningPlayer())
```

---

## 신규 생성 / 수정 파일

### 신규
| 파일 | 내용 |
|------|------|
| `ExCore/Source/ExCoreRuntime/Util/Actor/ExActorUtil.h/.cpp` | `FindOwnerPawn(Component*)`, `FindOwnerPawn(Actor*)` 유틸 함수 |

### 수정
| 파일 | 변경 내용 |
|------|------|
| `ExPlayerStatsViewModel.h/.cpp` | `CurrentScore`, `MatchTimeRemaining` → `UPROPERTY(FieldNotify, Setter, Getter)` 전환 |
| `ExRunnerStatsViewModel.h/.cpp` | `CurrentSpeed` → `FieldNotify` 전환, `GetCurrentSpeedText` 추가, `AutoInitialize` 추가 |
| `ExRunnerStatComponent.h/.cpp` | 타이머 기반 폴링 구조 도입, `UMoverComponent::GetVelocity()` 사용, `ExActorUtil` 적용 |
| `ExRunnerMovementComponent.cpp` | Tick 속도 업데이트 제거, `ExActorUtil` 적용 |

---

## 핵심 교훈

1. **MVVM View Bindings Target**은 반드시 `UPROPERTY(FieldNotify, Setter, Getter)`로 선언된 변수여야 함. UFUNCTION은 Source만 가능
2. **Mover 시스템**에서 `APawn::GetVelocity()`는 0을 반환. `UMoverComponent::GetVelocity()` 사용 필수
3. **On Initialized** vs **On Activated**: ViewModel 접근은 On Activated에서 해야 함
4. **Core → Feature 역참조** 방지: 탐색 로직을 Feature 모듈 함수로 캡슐화 (`AutoInitialize`)
