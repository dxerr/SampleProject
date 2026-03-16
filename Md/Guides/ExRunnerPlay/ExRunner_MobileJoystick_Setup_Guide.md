# 모바일 조이스틱 & 스와이프 입력 시스템 설정 가이드

> **최종 업데이트:** 2026-03-16  
> **구현 방식:** `ExBaseTouchPadWidget` (C++) + `ExRunnerInputViewModel` (C++) + `ExRunnerInputComponent` (C++)

---

## 전체 아키텍처 요약

```
[WBP_ExTouchPad]
  └─ ExBaseTouchPadWidget (C++ 베이스)
       │  NativeOnTouchMoved()
       │    ├─ FrameDelta       : 픽셀 단위 프레임 이동량
       │    ├─ LocalPosition    : 위젯 내 터치 픽셀 좌표
       │    └─ NormalizedOffset : 패드 중앙 기준 -1.0 ~ 1.0 정규화값
       │
       └─ BP_OnTouchPadMoved(FrameDelta, LocalPosition, NormalizedOffset)
            │
            ▼
[ExRunnerInputViewModel]
  ├─ OnTouchPadMoved(FrameDelta, LocalPosition, NormalizedOffset)
  │    ├─ [X축] NormalizedOffset.X → RequestLookAction(X) → Yaw 회전
  │    └─ [Y축] RelativeY 계산 → 임계값 비교 → Jump / Slide 이벤트
  └─ OnTouchPadReleased() → 상태 초기화 + 강제 해제
       │
       ▼
[ExRunnerInputComponent]
  ├─ RequestLookAction(float)    → OnLookRequested 브로드캐스트
  ├─ RequestJumpAction(bool)     → InjectedInputStates + 직접 Broadcast(false)
  └─ RequestSlideAction(bool)    → InjectedInputStates + 직접 Broadcast(false)
       │
       ▼
[캐릭터 BP] OnJumpRequested / OnSlideRequested / OnLookRequested 델리게이트 구독
```

---

## 1. WBP_ExTouchPad 위젯 구성

### 1.1 클래스 설정
- `WBP_ExTouchPad` 위젯 블루프린트의 **Parent Class**를 `ExBaseTouchPadWidget`으로 설정

### 1.2 디자이너 구성
| 위젯 이름 | 설명 | Visibility |
|---|---|---|
| `BgImage` | 터치 감지용 투명 배경 (Name이 `BgImage`여야 C++이 자동 바인딩) | **`Visible`** (필수) |
| `ThumbImage` | 터치 포인터를 따라다니는 썸네일 이미지 | **`Hit Test Invisible`** |

> [!IMPORTANT]
> `BgImage`의 Visibility는 반드시 **`Visible`** 이어야 터치 이벤트를 수신합니다.  
> `ThumbImage`는 **`Hit Test Invisible`** 이어야 BgImage의 터치 판정을 방해하지 않습니다.

### 1.3 ThumbSize 설정
- `WBP_ExTouchPad` 위젯 Details 패널에서 **`Thumb Size`** 값을 ThumbImage의 실제 크기와 동일하게 설정 (기본: 50×50)

---

## 2. ViewModel 설정

### 2.1 ViewModel 추가 (Viewmodels 패널)
1. `WBP_ExTouchPad` 열기 → 상단 메뉴 `Window → Viewmodels`
2. `+ Viewmodel` 클릭 → `Ex Runner Input View Model` 선택
3. **`Creation Type`을 `Create Instance`로 변경** ⚠️필수

> [!CAUTION]
> 좌측 Variables에 직접 만든 ExRunnerInputViewModel 변수가 있다면 **반드시 삭제**하세요.  
> 항상 Viewmodels 카테고리 하단의 **초록색 자동 생성 변수**를 사용해야 합니다.

### 2.2 이벤트 그래프 노드 연결

**Event Graph에서 총 3개 이벤트를 연결합니다:**

```
[Event On Touch Pad Moved]          [ExRunnerInputViewModel]
  Out Frame Delta      ──────────►  Frame Delta
  Out Local Position   ──────────►  Local Position
  Out Normalized Offset ─────────►  Normalized Offset   ← 3번째 파라미터
  (ExRunnerInputViewModelRef)──────► Target

[Event On Touch Pad Ended]          [ExRunnerInputViewModel]
  (실행 핀) ─────────────────────►  On Touch Pad Released
  (ExRunnerInputViewModelRef)──────► Target
```

> [!WARNING]
> 3번째 파라미터 핀 이름이 `Normalized Offset`임을 확인하세요.  
> 이전 버전의 `Touch Pad Size` 핀 이름이 남아있다면 **`Refresh Node`** 로 갱신 필요.

---

## 3. ExRunnerInputComponent 설정 (캐릭터 BP)

캐릭터 블루프린트에 `ExRunnerInputComponent`가 붙어 있어야 합니다.

### 3.1 GameModeDataSet 할당
- `ExRunnerInputComponent` 디테일 패널 → `ExInput|Runner|Settings`
- **`Game Mode Data Set`** 항목에 `DA_ExGameModeDataSet` 에셋 연결

### 3.2 DA_ExGameModeDataSet 설정값
| 파라미터 | 경로 | 설명 |
|---|---|---|
| `RunnerLookSensitivity` | `GameMode > Runner` | X축 회전 민감도 (기본: 1.0) |
| `SwipeActivationPercentage` | `GameMode > Runner` | 스와이프 발동 임계 비율 (기본: 0.3 = 30%) |

---

## 4. 입력 동작 상세

### 4.1 X축 (좌우 드래그) → Yaw 회전
- `NormalizedOffset.X` (-1.0 ~ 1.0, 패드 중앙 기준 절대 위치) 사용
- `RequestLookAction(NormalizedOffset.X)` → `RunnerLookSensitivity` 곱셈 → `OnLookRequested` 브로드캐스트

### 4.2 Y축 (상하 드래그) → Jump / Slide
**RelativeY 방식** (터치 시작점 기준 상대 이동량):
```
RelativeY = NormalizedOffset.Y(현재) - SwipeStartNormY(터치 첫 프레임)
```

| 방향 | 조건 | 이벤트 |
|---|---|---|
| 위로 드래그 | RelativeY ≤ -SwipeActivationPercentage | `RequestJumpAction(true)` 발동, 임계 이탈/Release시 `false` |
| 아래로 드래그 | RelativeY ≥ +SwipeActivationPercentage | `RequestSlideAction(true)` 발동, 임계 이탈/Release시 `false` |
| 임계값 미달 | - | 아무 이벤트 없음 |

> [!NOTE]
> **Jump**: Hold 방식 - 임계값 이상 유지 시 `true`, 임계 이탈 또는 Release 시 `false`  
> **Slide**: 임계값 진입 시 `true` 1회, 임계 이탈 또는 Release 시 `false` 1회

### 4.3 false 이벤트 전달 메커니즘
`RequestJumpAction(false)` / `RequestSlideAction(false)` 호출 시:
1. `InjectedInputStates`에서 제거 (Enhanced Input 주입 중단)
2. `OnJumpRequested.Broadcast(false)` / `OnSlideRequested.Broadcast(false)` **직접 호출**

> [!IMPORTANT]
> Enhanced Input의 `Completed` 이벤트는 `Value.Get<bool>()`이 `true`를 반환할 수 있어 불안정합니다.  
> 때문에 false는 Request 함수에서 직접 Broadcast하는 방식을 사용합니다.

---

## 5. 디버그 방법

### 5.1 출력 로그 키워드

| 키워드 | 의미 |
|---|---|
| `[ExTouchPadMoved]` | 매 프레임 터치 상태 (NormX, NormY, RelY, Threshold) |
| `[ExJump] *** ACTIVATED ***` | 점프 임계값 돌파 → `RequestJumpAction(true)` |
| `[ExJump] *** DEACTIVATED ***` | 점프 해제 → `RequestJumpAction(false)` |
| `[ExSlide] *** ACTIVATE ***` | 슬라이드 임계값 돌파 → `RequestSlideAction(true)` |
| `[ExSlide] *** RESTORE ***` | 슬라이드 해제 → `RequestSlideAction(false)` |
| `[ExRunnerInput] RequestSlideAction(false) → 직접` | false Broadcast 직접 호출 확인 |

### 5.2 화면 디버그 텍스트
`TAG_Ex_Debug_Speed` 태그가 활성화된 경우 화면에 실시간으로 표시됩니다:
```
[UI Swipe] NormX:+0.42 | RelY:-0.35 | Thr:[-0.30 / 0.30]
```

### 5.3 SwipeActivationPercentage 런타임 조정
- 에디터에서 `DA_ExGameModeDataSet` 열기 → `GameMode > Runner > Swipe Activation Percentage` 조정
- 값이 작을수록 이른 발동 (0.05 ~ 1.0, 기본: 0.3)

---

## 6. 관련 파일 목록

| 파일 | 역할 |
|---|---|
| `ExBaseTouchPadWidget.h/.cpp` | 터치 이벤트 수신, 정규화 계산, BP 이벤트 브로드캐스트 |
| `ExRunnerInputViewModel.h/.cpp` | 터치 → 점프/슬라이드/회전 로직 변환 |
| `ExRunnerInputComponent.h/.cpp` | Enhanced Input 주입 + 델리게이트 브로드캐스트 |
| `ExGameModeDataSet.h` | RunnerLookSensitivity, SwipeActivationPercentage 설정 |
| `DA_ExGameModeDataSet` (에셋) | 런타임 설정값 조정 |
| `WBP_ExTouchPad` (블루프린트) | ExBaseTouchPadWidget 상속 위젯 |
