# [버그 리포트] 안드로이드 빌드용 Mover 자동 전진 및 좌우 스와이프 충돌 현상

## 이슈 요약
- **1차 증상**: `RequestMoveAction(0, 1)` 주입 코드를 제거하자 에디터에서는 전진을 잘 하지만 안드로이드 빌드에서는 전진하지 못하고 캐릭터가 멈춤.
- **2차 증상**: 에디터와 안드로이드 모두 주입을 부활시키자 전진은 다시 동작하지만, 좌우 레인 스와이프(조향) 입력이 무시되는 멱등성 충돌 발생.

## 원인 분석 (진짜 원인 발견)
1. **스와이프 우선순위 박탈 (2차 증상의 원인)**: 언리얼 Enhanced Input 시스템에서 `InjectInputVectorForAction`을 사용해 인위적으로 입력값을 꽂아 넣으면, **해당 Action에 매핑된 실제 하드웨어 터치(스와이프/조이스틱) 입력은 시스템이 무시(Suppress)** 해 버립니다. 이 때문에 X축 값을 보존하려고 시도해도, Enhanced Input 단에서 조작 자체를 차단해버려 영원히 `X=0` 상태의 굴레에 빠졌습니다.
2. **안드로이드에서만 전진하지 못했던 이유 (1차 증상의 원인)**: 
   안드로이드 모바일 환경은 `MoveAction`에 '가상 조이스틱' 장치가 기본 할당되어 있습니다. 이 가상 조이스틱은 스와이프하지 않을 때 항상 `(0, 0)` 값을 뿜어냅니다.
   Mover 컴포넌트의 초깃값으로 설정되어 있는 `기본 Input Producer(예: UCommonMoverInputProducer)`가 이 `(0, 0)` 값을 읽어들여 폰의 `DirectionalIntent`를 0으로 맞춥니다.
   문제는 객체 생성/초기화 순서상 `UExRunnerMovementComponent`가 배열에 먼저 등록되고, 컨테이너 폰의 기본 Producer가 늦게 등록되면서 **기본 Producer가 우리 시스템의 `Pure Pursuit(경우점 추적 전진 명령)`을 최후에 덮어써버렸던 것**입니다! (PC 에디터에서는 가상 조이스틱이 없어 `0,0`이 강제발생하지 않아 멀쩡했습니다.)

## 해결 방법 (최종 수정안)
1. **문제의 주입 코드 영구 삭제 (`ExRunnerInputComponent.h/cpp` 원상복구)**:
   가상 조작값과 실제 스와이프 조작값이 충돌하는 주입 구조 자체를 폐기했습니다. 이제 플레이어의 순수한 터치 입력만이 `MoveAction`을 구동하여 좌우 이동이 정상 작동합니다.
2. **Mover 우선순위 강제 재정렬 (`ExRunnerMovementComponent::TickComponent`)**:
   `InputProducers` 배열 끝에 `UExRunnerMovementComponent(this)`가 항상 마지막 인덱스로 존재하도록 `Remove` -> `Add` 로직을 Tick 단위로 배치했습니다.
   이로 인해 Mover의 기본 Producer가 아무리 화면 터치가 없어서 `(0, 0)`을 내뿜어도, 우리가 구현한 `ProduceInput_Implementation`이 최종적으로 `ForwardDir(Pure Pursuit)`을 덧칠하게 되어 안드로이드에서도 전진이 보장됩니다.

## 결과
- 터치 입력이 씹히는 구조 결함 완벽 해소 (좌우 스와이프 가능).
- 안드로이드 환경에서도 Mover의 가상 조이스틱 (0,0) 현상을 무시하고 자동 전진 동작 성공.
