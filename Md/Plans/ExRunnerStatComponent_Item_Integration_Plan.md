# 러너 스탯 & 아이템 연동 시스템 구현 계획

> **버전:** v1.0  
> **대상 엔진:** Unreal Engine 5  
> **프로젝트:** ExFrameWork  
> **작성일:** 2026-03-26  
> **의존 문서:**  
> - ExFrameWork_Item_System_Architecture.md (v1.2)  
> - ExFrameWork_Guidelines.md  

`UExRunnerStatComponent`에 코인 갯수 / 스프린트 남은 시간을 추가하고, 아이템 획득 이벤트와 연동하여 UI까지 바인딩하는 전체 계획.

---

## 1. 설계 판단 요약

### 1.1 서버 권한(Authority) 분석

| 기능 | 서버 판정 필요 여부 | 이유 |
|---|---|---|
| **코인 갯수 증가** | **불필요** (기존 경로 유지) | `ExItemEffect_Score::Execute`가 이미 **서버에서만 호출**되어 `PS->AddScore()`를 처리. StatComponent는 해당 결과를 **구독하여 표시만** 하면 됨 |
| **스프린트 활성화/지속시간** | **불필요** (기존 경로 유지) | `ExItemEffect_Buff::Execute`가 이미 **서버에서만 호출**. `BuffTag` 이벤트도 서버에서 발생하며, StatComponent의 타이머는 **로컬 표현 계층**. 실제 이동속도 변경은 `ExRunnerMovementComponent`가 서버에서 처리 |

> **결론:** 아이템 획득의 서버 판정은 **이미 `AExItemPickupBase::ServerPickUp`에서 보장**되고 있다. StatComponent는 순수 표현(Presentation) 레이어이므로 추가적인 서버 권한 로직이 불필요하다.

### 1.2 데이터 흐름

```
[서버] AExItemPickupBase::ServerPickUp
  → UExItemEffect_Score::Execute → PS->AddScore() + Broadcast TAG_Ex_Item_PickedUp_Score
  → UExItemEffect_Buff::Execute  → Broadcast TAG_Ex_Buff_SpeedUp (Magnitude, Duration)
       ↓
[ExGameplayEventSubsystem] (World Subsystem)
       ↓ (구독)
[UExRunnerStatComponent]
  ├─ OnScorePickedUp → CoinCount += ScoreAmount → Broadcast OnCoinCountChanged
  └─ OnSpeedUpBuff   → SprintRemaining = Duration → StartSprintTimer
                        → InputComponent->RequestSprintAction(true)
                        타이머 만료 시:
                        → InputComponent->RequestSprintAction(false) → Broadcast OnSprintTimeChanged
       ↓ (UI 바인딩)
[WBP_ExRunnerSpeedBar] → 코인 갯수 + 스프린트 잔여 시간 표시
```

---

## 2. 변경 상세

### 2.1 FExGameplayEventPayload 확장 (ExCore)

#### [MODIFY] ExGameplayEventSubsystem.h
- 경로: `Plugins/GameFeatures/ExCore/Source/ExCoreRuntime/Events/ExGameplayEventSubsystem.h`

`FExGameplayEventPayload`에 `Duration` 필드 추가 (기존 코드에 TODO로 명시되어 있던 설계 의도):

```cpp
/** 추가 데이터 (선택적) */
UPROPERTY(BlueprintReadWrite, Category = "Ex|Event")
float OptionalValue = 0.0f;

/** 버프/효과 지속 시간 (선택적, 초) */
UPROPERTY(BlueprintReadWrite, Category = "Ex|Event")
float Duration = 0.0f;
```

---

#### [MODIFY] ExItemEffect_Buff.cpp
- 경로: `Plugins/GameFeatures/ExCore/Source/ExCoreRuntime/Items/Effects/ExItemEffect_Buff.cpp`

페이로드에 Duration 전달:

```cpp
Payload.OptionalValue = Magnitude;
Payload.Duration = Duration; // [추가] 지속 시간 전파
```

---

### 2.2 UExRunnerStatComponent 확장 (ExRunnerPlay)

#### [MODIFY] ExRunnerStatComponent.h
- 경로: `Plugins/GameFeatures/ExRunnerPlay/Source/ExRunnerPlayRuntime/Components/ExRunnerStatComponent.h`

추가할 멤버 및 인터페이스:

```cpp
// ── 델리게이트 ──
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCoinCountChanged, int32, NewCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSprintTimeChanged, float, RemainingTime);

// ── 코인 ──
UPROPERTY(BlueprintAssignable, Category = "Runner|Stats|Events")
FOnCoinCountChanged OnCoinCountChanged;

UFUNCTION(BlueprintPure, Category = "Runner|Stats")
int32 GetCoinCount() const;

UFUNCTION(BlueprintCallable, Category = "Runner|Stats")
void AddCoinCount(int32 Amount);

// ── 스프린트 ──
UPROPERTY(BlueprintAssignable, Category = "Runner|Stats|Events")
FOnSprintTimeChanged OnSprintTimeChanged;

UFUNCTION(BlueprintPure, Category = "Runner|Stats")
float GetSprintRemainingTime() const;

UFUNCTION(BlueprintCallable, Category = "Runner|Stats")
void ActivateSprint(float Duration);

// ── private 영역 ──
int32 CoinCount = 0;
float SprintRemainingTime = 0.f;
FTimerHandle SprintTimerHandle;
TWeakObjectPtr<UExRunnerInputComponent> CachedInputComponent;

// 이벤트 핸들러 (ExGameplayEventSubsystem 구독용)
UFUNCTION()
void OnScorePickedUp(FGameplayTag Tag, const FExGameplayEventPayload& Payload);

UFUNCTION()
void OnSpeedUpBuff(FGameplayTag Tag, const FExGameplayEventPayload& Payload);

// 스프린트 잔여 시간 차감 (UpdateStats 내에서 호출)
void UpdateSprintTimer();
```

#### [MODIFY] ExRunnerStatComponent.cpp
- 경로: `Plugins/GameFeatures/ExRunnerPlay/Source/ExRunnerPlayRuntime/Components/ExRunnerStatComponent.cpp`

구현 순서:

1. **BeginPlay**:
   - `ExGameplayEventSubsystem`의 `TAG_Ex_Item_PickedUp_Score` 이벤트 구독 → `OnScorePickedUp` 바인딩
   - `ExGameplayEventSubsystem`의 `TAG_Ex_Buff_SpeedUp` 이벤트 구독 → `OnSpeedUpBuff` 바인딩
   - `UExRunnerInputComponent`를 Owner Pawn에서 찾아 캐싱

2. **OnScorePickedUp(Tag, Payload)**:
   - `Payload.OptionalValue`(ScoreAmount)를 `int32`로 캐스팅
   - `AddCoinCount(Amount)` 호출

3. **AddCoinCount(Amount)**:
   - `CoinCount += Amount`
   - `OnCoinCountChanged.Broadcast(CoinCount)`

4. **OnSpeedUpBuff(Tag, Payload)**:
   - `Payload.Duration`을 받아 `ActivateSprint(Duration)` 호출

5. **ActivateSprint(Duration)**:
   - `SprintRemainingTime = Duration` (이미 활성 중이면 시간 갱신/리셋)
   - `CachedInputComponent->RequestSprintAction(true)` 호출
   - `OnSprintTimeChanged.Broadcast(SprintRemainingTime)`

6. **UpdateStats() (기존 타이머 함수에 추가)**:
   - `UpdateSprintTimer()` 호출

7. **UpdateSprintTimer()**:
   - `SprintRemainingTime > 0`이면:
     - `SprintRemainingTime -= StatPollInterval`
     - `OnSprintTimeChanged.Broadcast(SprintRemainingTime)`
     - 만약 `SprintRemainingTime <= 0`이면:
       - `SprintRemainingTime = 0`
       - `CachedInputComponent->RequestSprintAction(false)`
       - `OnSprintTimeChanged.Broadcast(0.f)`

### 2.3 UI 바인딩 (Blueprint)

#### [MODIFY] WBP_ExRunnerSpeedBar (Blueprint Widget)
- 경로: `Plugins/GameFeatures/ExRunnerPlay/Content/UI/Parts/WBP_ExRunnerSpeedBar.uasset`

> ⚠️ 이 위젯은 `.uasset`(블루프린트)이므로 **에디터에서 직접 수정**이 필요

에디터에서 수행할 작업:
1. `WBP_ExRunnerSpeedBar`에 코인 갯수 표시용 `TextBlock` 추가
2. 스프린트 잔여 시간 표시용 `ProgressBar` 또는 `TextBlock` 추가
3. `StatComponent`의 `OnCoinCountChanged` / `OnSprintTimeChanged` 델리게이트에 바인딩

---

## 3. 검증 체크리스트

### 에디터 테스트
- [ ] PIE 실행 후 코인 획득 시 로그에 `CoinCount` 변경 로그 출력 확인
- [ ] `OnCoinCountChanged` 이벤트 정상 발행 확인
- [ ] SpeedUp 버프 획득 시 캐릭터가 즉시 스프린트 상태로 전환 확인
- [ ] 설정된 Duration 이후 자동으로 스프린트 해제 확인
- [ ] 버프 중 추가 버프 획득 시 남은 시간 갱신(리셋) 확인
- [ ] UI(WBP_ExRunnerSpeedBar)에서 코인/스프린트 타이머 표시 실시간 갱신 확인

---

## 4. 변경 파일 요약

| 파일 | 모듈 | 변경 내용 |
|---|---|---|
| `ExGameplayEventSubsystem.h` | ExCore | `FExGameplayEventPayload`에 `Duration` 필드 추가 |
| `ExItemEffect_Buff.cpp` | ExCore | 페이로드에 `Duration` 값 전달 |
| `ExRunnerStatComponent.h` | ExRunnerPlay | 코인/스프린트 변수, 델리게이트, 이벤트 핸들러 추가 |
| `ExRunnerStatComponent.cpp` | ExRunnerPlay | 이벤트 구독, 코인 갯수 관리, 스프린트 타이머 관리 구현 |
| `WBP_ExRunnerSpeedBar` | ExRunnerPlay (BP) | 코인/스프린트 UI 바인딩 (에디터 수동 작업) |

---

## 변경 이력

| 날짜 | 버전 | 변경 내용 |
|------|------|-----------|
| 2026-03-26 | v1.0 | 초안 작성. 코인 갯수/스프린트 Duration/이벤트 구독/UI 바인딩 설계 |
