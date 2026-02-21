# Phase 1: ExDebugStateSubsystem — 전역 디버그 상태 관리

> 최종 갱신: 2026-02-21

---

## 1. 설계 목적

| 요구사항 | 해결 방안 |
|----------|----------|
| 레벨 전환 간 상태 유지 | `UGameInstanceSubsystem` 사용 |
| Feature에서 Core를 참조 (역방향 금지) | `FGameplayTag` 기반 상태 관리 |
| 상태 변경 알림 | 기존 `UExGameplayEventSubsystem` Pub/Sub 활용 |
| Shipping 빌드 제외 | `ShouldCreateSubsystem`에서 `UE_BUILD_SHIPPING` 체크 |

## 2. 클래스 구조

### UExDebugStateSubsystem (GameInstanceSubsystem)
```cpp
// 핵심 인터페이스
void SetCheatEnabled(FGameplayTag CheatTag, bool bEnabled);
bool IsCheatEnabled(FGameplayTag CheatTag) const;
bool ToggleCheat(FGameplayTag CheatTag);  // 반환: 새 상태
void SetCheatValue(FGameplayTag CheatTag, float Value);
float GetCheatValue(FGameplayTag CheatTag) const;
void ResetAllStates();
void PrintAllStates() const;
```

### 내부 데이터 구조
```
TMap<FGameplayTag, FExDebugCheatState> CheatStates;

FExDebugCheatState:
  - bool bEnabled     (Toggle용)
  - float Value       (Slider용)
  - int32 SelectedIndex (Select용)
```

## 3. 이벤트 연동 흐름

```
CheatManager(콘솔) → DebugStateSubsystem → ExGameplayEventSubsystem
                         ↓ [Tag + Payload]
                    각 Actor/System이 Subscribe하여 반응
```

- `BroadcastStateChange()` 내부에서 `GameInstance→World→ExGameplayEventSubsystem` 경로로 접근
- `FExGameplayEventPayload.OptionalValue`에 `bEnabled ? 1.f : 0.f` 전달

## 4. 디버그 태그 목록

| 태그 | 문자열 | 용도 |
|------|--------|------|
| `TAG_Ex_Debug_Path` | `Ex.Debug.Path` | 경로 시각화 |
| `TAG_Ex_Debug_Chunk` | `Ex.Debug.Chunk` | 청크 경계/상태 |
| `TAG_Ex_Debug_Slope` | `Ex.Debug.Slope` | 꽈배기(경사) 디버그 |
| `TAG_Ex_Debug_Speed` | `Ex.Debug.Speed` | 속도 디버그 |
| `TAG_Ex_Debug_GodMode` | `Ex.Debug.GodMode` | 무적 모드 |
| `TAG_Ex_Debug_Collision` | `Ex.Debug.Collision` | 충돌 시각화 |

> Feature 모듈에서 필요한 태그는 해당 Feature의 `.h`에 추가 정의

## 5. UExCheatManager 콘솔 명령

| 명령 | 기능 |
|------|------|
| `ExDebug [카테고리]` | `Ex.Debug.[카테고리]` 태그 토글 |
| `ExDebugStatus` | 모든 상태 로그 출력 |
| `ExDebugReset` | 전체 상태 초기화 |

## 6. Feature 연동 예시

ExRunnerPlay에서 `TAG_Ex_Debug_Slope` 구독:
```cpp
void UExPathManager::BeginPlay()
{
    if (auto* ES = GetWorld()->GetSubsystem<UExGameplayEventSubsystem>())
    {
        ES->GetEventDelegate(TAG_Ex_Debug_Slope)
           .AddDynamic(this, &UExPathManager::OnDebugSlopeChanged);
    }
}

void UExPathManager::OnDebugSlopeChanged(FGameplayTag Tag, const FExGameplayEventPayload& Payload)
{
    bDebugSlopeForced = Payload.OptionalValue > 0.f;
}
```
