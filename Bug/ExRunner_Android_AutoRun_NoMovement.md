# [버그 리포트] 안드로이드 빌드 시 Mover 전진 안 함 (Auto-Run)

## 이슈 요약
에디터 환경에서는 정상적으로 캐릭터가 자동 전진(Auto-Run)을 수행하지만, 안드로이드(모바일) 환경 또는 Shipping 빌드에서는 캐릭터가 달리지 않고 제자리에 서있는 현상 발생.

## 원인 분석
1. **의도적인 코드 제거의 여파**: 기존에 `ExRunnerMovementComponent::ProduceInput_Implementation` 안에서 `InputComp->RequestMoveAction(FVector2D(0.0f, 1.0f));`를 매 프레임 주입하던 로직을 제거함. (좌우 터치/조이스틱 등 Enhanced Input의 X축 값이 멱등성에 의해 `0.0`으로 덮어씌워져 조향이나 차선 변경이 씹히는 구조적 결함 때문)
2. **모바일 Mover/Enhanced Input의 Sleep 특성**: 키보드 입력이 없고 가상 조이스틱 터치 이벤트가 발생하지 않는 모바일 빌드에서는 Enhanced Input의 `MoveAction`이 비활성화(Zero) 상태를 반환함. Mover는 기본 Input Producer 통신 단계에서 별개의 조작 인텐트(Intent)가 없거나 갱신된 입력 패킷이 없다고 판단하여 사실상 시뮬레이션 파이프라인에서 액션을 유동적으로 정지하거나 애니메이션 State(Idle) 전환에 묶임. 
결과적으로 Pure Pursuit이 바라볼 올바른 경로(ForwardDir)를 계산하더라도, 시스템이 Awake 상태로 입력 커맨드를 실행하지 않음.

## 해결 방법
**스와이프 보존 로직 + Y 강제전진 동시 합성 방식** 도입
1. **X축 값 보존 캐싱**: `UExRunnerInputComponent::NativeOnMoveAction`에서 플레이어가 가상 조이스틱이나 화면을 스와이프했을 때 발생한 X축과 Y축 값을 `LastReceivedMoveInput`에 보존.
2. **`InjectAutoForwardInput()` 신설**: 안전하게 Y축을 `1.0` (전진)으로 강제하되, X축은 보존된 `LastReceivedMoveInput.X`를 합성하여 주입하는 새로운 로직 개설.
3. **Mover 호출부 변경**: `ProduceInput` 호출 타임에서 기존처럼 `(0, 1)` 직접 강제가 아닌 `InputComp->InjectAutoForwardInput()`을 쏘도록 교체. AutoRun 모드일 경우에만 작동하도록 안전 조건 추가.

## 결과
- 안드로이드 환경에서도 Enhanced Input이 지속된 "액션 진행(Awake)"을 Mover에게 보고하게 되어 정상적으로 자동 전진 수행 가능.
- 기존에 우려되었던 좌우 차선변경(X값 터치오차/씹힘 현상) 버그가 완벽하게 복구 및 보호됨.
