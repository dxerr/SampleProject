# CommonUI Mouse/Touch 플래핑 및 F8 입력 방해 문제 해결

## 개요
- **일자**: 2026-03-25
- **키워드**: CommonUI, CommonInputSubsystem, bUseMouseForTouch, F8 Eject, Input, Flapping
- **목적**: 에디터 내에서 플레이(PIE) 도중 F8(Eject) 탈출 시 마우스 드래그 및 W,A,S,D 입력이 무반응하며 시스템 로그에 디바이스 전환 스팸이 발생하는 버그 기록 및 원인 파악

## 증상
- 인게임 혹은 PIE 구동 중 F8을 눌러 Eject 상태로 전환해 디버깅을 시도할 시 마우스 우클릭을 동반한 시점 이동이 씹히는 증상 발견.
- 아래와 같이 터치와 마우스 사이에서 무한 루프 스팸 로그가 쏟아짐.
  ```text
  LogCommonInput: UCommonInputSubsystem::RecalculateCurrentInputType(): Using Mouse
  LogCommonInput: UCommonInputSubsystem::RecalculateCurrentInputType(): Using Touch
  ```

## 문제점 및 원인 분석
- `Config/DefaultInput.ini`에 **`bUseMouseForTouch=True`**가 설정된 상태였음.
- 이 옵션이 켜지면 마우스 입력이 실제 마우스 장치 이벤트와 모바일 터치 시뮬레이트 이벤트를 동시에 발생시킴.
- 프로젝트 내 플러그인인 `CommonUI`의 `UCommonInputSubsystem`은 어떤 종류의 입력 데이터가 들어오는지 실시간으로 모니터링하여 현재의 UI 상호작용 방식(아이콘 모양, 포커스 등)을 업데이트하는 특성이 있음.
- 따라서 하나의 마우스 동작이 "마우스 입력" -> "터치 모드로 전환" -> "그런데 마우스 하드웨어 이벤트도 발생" -> "마우스 모드 전환" -> "터치 시뮬레이팅 발생" -> 무한 반복 의 루프로 진입하여 입력 파이프라인과 포커스 권한을 마비시키게 됨.

## 해결 방법
- `Config/DefaultInput.ini`에서 **`bUseMouseForTouch=False`**로 옵션을 끄도록 변경하여 문제 해결.
- (주의사항) 해당 옵션을 끄게 되면 피시 마우스 조작만으로 모바일 터치 전용 기능이나 터치 패드를 에뮬레이션할 수는 없게 됨. 모바일 터치 테스트 시에는 Mobile Preview PIE 등 다른 모바일 전용 시뮬레이팅 환경을 활용해야 함.

## 결과
설정을 끈 후 에디터를 재시작 혹은 PIE 구동 시 위 스팸 로그가 더 이상 출력되지 않으며 F8을 통한 포커스 이동 및 조작이 원활하게 작동함.

## [추가] 에디터 내 가상 터치패드(WBP_ExTouchPad) 마우스 테스트 지원
- **이슈**: `bUseMouseForTouch`를 `False`로 변경하면 엔진 단에서 마우스->터치 변환을 하지 않으므로 에디터 플레이 중 터치패드를 마우스로 조작할 수 없게 됨.
- **해결 방안 (우회)**: `UExBaseTouchPadWidget` C++ 베이스 클래스에 `#if WITH_EDITOR` 전처리기 지시문을 사용하여 마우스 이벤트(`NativeOnMouseButtonDown`, `NativeOnMouseMove`, `NativeOnMouseButtonUp`, `NativeOnMouseCaptureLost`)를 오버라이드.
- **핵심 포인트**: 마우스 클릭 시 포커스를 빼앗겨 캡처 루프가 중단되는 현상을 방지하기 위해 `SetIsFocusable(true);`를 활성화하고, 반환 값에 `SetUserFocus(TakeWidget(), EFocusCause::Mouse)`를 적용하여 강제로 마우스 포커스와 캡처를 해당 위젯에 고정시킴으로써 모바일 터치와 완벽히 동일한 테스트 경험을 제공함.
