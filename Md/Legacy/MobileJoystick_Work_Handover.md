# 모바일 조이스틱 작업 현황 및 인수인계

**최종 업데이트:** 2026-03-16  
**관련 가이드 문서:** `Guides/ExRunnerPlay/ExRunner_MobileJoystick_Setup_Guide.md`

---

## ✅ 완료된 구현 내용 (2026-03-16 기준)

### 1. 터치 입력 파라미터 정규화 (`ExBaseTouchPadWidget`)
- `NativeOnTouchMoved()`에서 3가지 값을 계산하여 BP 이벤트로 전달
  - `FrameDelta`: 픽셀 단위 프레임 이동량
  - `LocalPosition`: 위젯 내 터치 픽셀 좌표
  - `NormalizedOffset`: **패드 중앙 기준 -1.0 ~ 1.0 정규화값** (핵심)

### 2. 스와이프 로직 재구현 (`ExRunnerInputViewModel`)
- **RelativeY 방식**: 터치 시작점(`SwipeStartNormY`) 기준 상대 이동량으로 임계값 판정
  - 절대값 누적 방식의 방향 오진입 버그 해결
  - 시작 위치와 무관하게 실제 드래그한 만큼만 발동
- **X축 (회전)**: `NormalizedOffset.X` → `RequestLookAction()` → Yaw 회전
- **Y축 (점프)**: RelativeY ≤ -Threshold → Hold 방식 Jump (임계 이탈 시 자동 해제)
- **Y축 (슬라이드)**: RelativeY ≥ +Threshold → Active/Restore 2-상태 방식

### 3. 스와이프 임계값 DataSet 통합 (`ExGameModeDataSet`)
- `SwipeActivationPercentage` 속성 추가 (`GameMode|Runner` 카테고리)
- `ExRunnerInputComponent::GetSwipeActivationPercentage()` 로 조회
- `DA_ExGameModeDataSet`에서 런타임 변경 가능

### 4. 슬라이드 상태 동기화 개선 (`ExRunnerInputComponent`)
- `IsSlideInputActive()` 추가: `InjectedInputStates.Contains(SlideAction)` 직접 조회
  - `bIsSlideActive` 로컬 변수 제거 → sync drift 방지
- `RequestJumpAction(false)` / `RequestSlideAction(false)` 에서 **직접 Broadcast**
  - Enhanced Input `Completed` 이벤트가 `true`를 반환하는 스펙 문제 우회
  - Jump/Slide 양쪽 모두 동일 방식으로 통일

### 5. Enhanced Input 바인딩 정리 (`InitializeInputBindings`)
- Jump, Slide 모두 `Triggered` + `Started` 바인딩 (false는 Request 함수 직접 처리)

---

## 🔧 현재 남아있는 고도화 과제

| 항목 | 설명 | 우선순위 |
|---|---|---|
| 연속 점프 지원 | Hold 상태 유지 중 임계값 재진입 시 재점프 가능 여부 검토 | 중 |
| 멀티터치 분리 | 왼손(이동)/오른손(터치패드) 입력 분리 | 중 |
| 터치 시각 피드백 | ThumbImage 위치 보간 이동 (현재는 즉시 이동) | 낮음 |
| 스와이프 방향락 | X축 드래그 중 Y축 스와이프 억제 로직 | 낮음 |

---

## 📁 관련 수정 파일 목록

| 파일 | 변경 내용 |
|---|---|
| `ExGameModeDataSet.h` | `SwipeActivationPercentage` 속성 추가 |
| `ExRunnerInputComponent.h` | `GetSwipeActivationPercentage()`, `IsSlideInputActive()` 선언 |
| `ExRunnerInputComponent.cpp` | 위 함수 구현 + Request 함수에 직접 Broadcast(false) 추가 |
| `ExRunnerInputViewModel.h` | `bIsSlideActive` 제거, `bIsTouchPadActive`, `SwipeStartNormY` 추가 |
| `ExRunnerInputViewModel.cpp` | 전체 스와이프/입력 로직 재구현 |
| `DA_ExGameModeDataSet` (에셋) | SwipeActivationPercentage 값 설정 |
