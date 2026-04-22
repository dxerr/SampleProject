# 모바일 조이스틱 입력 구조 도입 제안서 (Proposal)

## 1. 개요 및 요구사항 분석
주인님의 지시에 따라 현재 버튼식 입력 방식을 보완하기 위해 **조이스틱 형태의 단일 입력 구조**를 도입하는 방안을 분석했습니다.
요구사항은 다음과 같습니다:
* **좌우 (X축):** PlayerController의 Yaw축 회전 (연속적인 Axis 데이터)
* **상하 (Y축):** 위로 스와이프/드래그 시 Jump, 아래로 스와이프/드래그 시 Slide 이벤트 발생 (단발성 Action 데이터)

## 2. 현재 아키텍처 (단일 파이프라인) 연계성
기존 `ExFrameWork_Input_System_Architecture.md`에 따르면, 모든 UI 입력은 **MVVM ViewBinding**을 거쳐 `UExRunnerInputViewModel`을 통해 `UExRunnerInputComponent`로 전달(`Request...Action`)되어야 합니다. 또한 이전 작업에서 `InjectInputForAction`을 활용한 안전한 입력 주입 파이프라인을 구축해둔 상태입니다.

따라서 새로운 조이스틱 입력 구조 역시 이 파이프라인을 준수하는 것이 아키텍처 일관성 유지에 가장 유리합니다.

---

## 3. 개발 접근 방식 제안

### 제안 1안: UMG + MVVM 기반 커스텀 터치 패드 (강력 권장)
엔진의 기본 가상 조이스틱을 사용하지 않고, 화면 우측 하단 등 지정된 영역을 터치할 때 작동하는 **커스텀 UMG 위젯**을 제작합니다.

**[작동 원리]**
1. **UMG (WBP_ExTouchPad):** 
   - 사용자가 터치 후 드래그(`OnTouchMoved`)할 때 화면의 X, Y 변화량(Delta)을 계산합니다.
   - X 델타값은 매 프레임 `ViewModel`의 `OnTouchPadMovedX(float AxisValue)`로 전송.
   - Y 델타값이 특정 임계점(Threshold)을 넘는 순간, 제스처로 판정하여 `ViewModel`의 `OnTouchPadSwipedUp()` 또는 `OnTouchPadSwipedDown()`을 호출. (호출 후 제스처 쿨다운 적용)
2. **ViewModel (`UExRunnerInputViewModel`):**
   - X 델타 수신 시: `InputComponent->RequestLookAction(X값)` 호출.
   - Swipe Up 수신 시: `InputComponent->RequestJumpAction(true)` (단일 프레임 펄스 주입).
   - Swipe Down 수신 시: `InputComponent->RequestSlideAction(true)` (단일 프레임 펄스 주입).
3. **Input Component (`UExRunnerInputComponent`):**
   - X축 입력에 대해 기존 로직대로 `AddControllerYawInput`을 간접 호출하거나 `OnLookRequested`를 브로드캐스트.
   - Jump/Slide는 기존과 동일하게 브로드캐스트.

**[장점]**
* **현재 아키텍처 완벽 준수:** UI 이벤트 -> MVVM -> InputComponent 흐름을 100% 따릅니다.
* **커스텀 용이성:** 민감도(Sensitivity), 스와이프 임계점(Threshold), 데드존(Deadzone) 등을 UMG나 ViewModel에서 직접 세밀하게 통제할 수 있습니다.
* **디자인 자유도:** 위젯 기반이므로 조이스틱의 시각적 요소(손가락 위치에 따라 동적으로 나타나는 조이스틱 등)를 완벽하게 커스텀하기 쉽습니다.

**[상세 UMG 위젯 구현 방안 - 버튼(UButton) 대신 UUserWidget 이벤트 오버라이드 사용]**
일반적인 UMG `Button` 위젯은 단순히 '클릭(Pressed/Released)'을 감지하는 데 특화되어 있어, 터치 후 손가락을 문지르는 연속적 드래그 액션(Delta 값 추출)을 자연스럽게 처리하기 어렵습니다. 따라서 다음과 같은 방식으로 전용 터치 패드 위젯(`WBP_ExTouchPad`)을 구성합니다.

1. **위젯 계층 구조 (Hierarchy):**
   - **Root:** `Size Box` 또는 `Canvas Panel` (원하는 터치 영역의 크기를 결정, 예: 화면 우측 하단 랩핑)
   - **Background:** `Image` (반투명한 터치 가능 영역 지정용 시각 객체, Visibility 설정: `Visible`)
   - **Thumb (선택 사항):** `Image` (사용자 터치 위치에 따라 동적으로 렌더링될 조이스틱 손잡이 그래픽, Visibility 설정: `Hit Test Invisible`)

2. **핵심 이벤트 오버라이드 (Event Override):**
   단순 버튼 위젯 컴포넌트를 배치하는 대신, **`UUserWidget` 베이스 클래스 자체에서 제공하는 기본 터치/마우스 메인 이벤트 함수들을 오버라이드**하여 저수준(Low-level) 입력 제어권을 가져옵니다.
   - **`OnTouchStarted` (또는 `OnMouseButtonDown`):** 
     - 터치가 시작된 캔버스 기준 좌표(Origin Location)를 레코딩합니다.
     - 포인터 인덱스를 추적하여 멀티 터치와의 간섭을 막고 `bIsTouching = true`로 상태를 플래깅합니다.
   - **`OnTouchMoved` (또는 `OnMouseMove`):**
     - 현재 위치와 이전 프레임 위치를 지속적으로 빼서 **Delta(변화량)** 쌍을 구합니다.
     - 이 Delta 값을 MVVM ViewBinding을 통해 ViewModel의 `OnTouchPadMovedX(Delta.X)` 등으로 전송합니다.
   - **`OnTouchEnded` (또는 `OnMouseButtonUp`):** 
     - 터치가 완료되거나 영역을 벗어나면 누적된 변수들을 초기화하고 핸들 상태를 해제(`false`)합니다.

### 제안 2안: 엔진 내장 가상 조이스틱 (Touch Interface Setup) 활용
프로젝트 세팅에서 설정할 수 있는 기본 터치 인터페이스 에셋(`TIS_MobileControls`)을 моди피하여 사용합니다.

**[작동 원리]**
1. **Touch Interface Asset:** 조이스틱의 X, Y축을 Enhanced Input의 특정 매핑 컨텍스트(IMC)에 직접 연결합니다. (예: X축 -> `IA_Look_X`, Y축 -> `IA_Virtual_Y`).
2. **Input Modifier / Trigger 구성:**
   - Y축(`IA_Virtual_Y`)에 대해 **Enhanced Input Trigger (Directional Deadzone / Pulse 등)**를 세팅하여, 위로 일정 수치 이상 올라가면 `IA_Jump`가 트리거된 것처럼 변환하는 엔진 내장 기능을 연구/적용.
3. **Input Component (`UExRunnerInputComponent`):**
   - 하드웨어 입력과 동일하게 Enhanced Input 콜백(`NativeOnJumpAction`)에서 자동으로 신호를 처리합니다.

**[장점]**
* C++ 및 UMG 코딩 양이 상대적으로 적습니다 (엔진 기본 기능 활용).

**[단점]**
* **아키텍처 우회:** UI 입력이지만 MVVM을 타지 않고 Enhanced Input 레벨에서 섞여 들어옵니다.
* **Y축 제스처 매핑의 난해함:** 아날로그 축(Y축)을 이산적인 액션(점프, 슬라이드)으로 변환하는 엔진의 기본 Trigger 조작이 직관적이지 않으며, 터치를 떼지 않고 위아래로 흔들 때 중복 트리거가 발생할 확률이 높습니다.

---

## 4. 결론 및 추천
기존에 구축된 **단일 입력 파이프라인 (MVVM -> InputComponent 기반 `InjectInputForAction`)과 가장 잘 부합하며 제어권이 확실한 "제안 1안 (UMG + MVVM 기반 커스텀 터치 패드)"을 채택할 것을 강력히 권장**합니다.

조이스틱 썸(Thumb)의 시각적 표현 여부(투명한 터치 영역으로 할지, 조이스틱 이미지를 그릴지)만 디자인 요소로 결정해 주시면, 기술적인 백엔드는 기존 아키텍처를 그대로 재사용하여 당장 안정적으로 구축할 수 있습니다.

주인님, **제안 1안**으로 진행할지, 아니면 다른 수정 사항이나 추가 의견이 있으신지 검토 부탁드립니다!
