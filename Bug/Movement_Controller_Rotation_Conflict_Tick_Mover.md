# [버그 보고서] 조이스틱 드래그 시 캐릭터 회전 덜덜거림 및 입력 무시 이슈 (Mover System vs Controller Rotation 충돌)

## 1. 이슈 요약 (Symptom)
- `MaxRunnerYawAngle`을 0도로 설정해도 화면을 50% 정도 드래그할 경우 캐릭터가 회전하려다 원점으로 돌아오려는 강력한 떨림(Jittering, 덜덜거림) 현상이 발생함.
- 관련된 블루프린트의 `Add Controller Yaw Input` 노드 연결을 끊었더니 덜덜거림은 멈추었으나, 조이스틱 각도 세팅 시 아예 직진만 할 뿐 캐릭터가 더 이상 회전(조향) 동작을 수행하지 않음.

## 2. 원인 분석 (Root Cause)
이번 버그는 3가지 중첩된 아키텍처 결함으로 인해 발생했습니다.

### A. 지연된 컴포넌트 주입으로 인한 C++ 델리게이트 바인딩 실패 (핵심 원인)
`ExRunnerInputComponent`가 언리얼 엔진의 `GameFeature` 플러그인 특성상 Mover 보다 한 틱 이상 늦게 캐릭터에 주입(Inject)되었습니다.
기존 `ExRunnerMovementComponent`의 초기화 코드는 단 한번만 해당 컴포넌트를 찾고 없으면 타이머를 종료해버렸기 때문에, C++ 단에서의 조이스틱 입력 수신 바인딩(`OnLookRequestedCallback`)이 영원히 체결되지 않았습니다. 즉 C++ 연산 상 `TargetLookYawOffset`은 영원히 `0.0f` 였습니다.

### B. 블루프린트 입력(BP)과 C++ 틱(Tick) 보간의 충돌
C++에서 바인딩이 실패하여 타겟 각도를 `0.0f`로 인식하고 있는 동안, 기존에 만들어둔 **블루프린트의 `IA_Look -> Add Controller Yaw Input` 노드가 단독으로** 컨트롤러 회전값을 매 프레임 추가하고 있었습니다.
- BP 로직: "사용자가 드래그했으니 컨트롤러 회전을 틀어라!"
- C++ 틱: "내 타겟 오프셋은 0.0 인데 왜 컨트롤러가 돌아가있지? 다시 0도로 원복(RInterpTo) 시켜라!"
이 상반된 두 명령이 매 프레임 싸우면서 극심한 덜덜거림이 발생했습니다.

### C. Mover 시스템과 Controller Rotation의 강제 동기화 충돌
Mover 시스템은 `ProduceInput`의 `OrientationIntent`를 기반으로 독자적인 매끄러운 캡슐 회전을 수행하려 합니다.
그러나 C++에서 `TargetPawn->bUseControllerRotationYaw = true;` 속성을 켜두었기 때문에, 엔진이 매 프레임마다 강제로 캐릭터 고개를 컨트롤러 방향(0도)으로 꺾어버리는 억압이 발생중이었습니다.

## 3. 해결 방안 (Resolution)
단순한 코드 수정이 아니라 **순수 Mover 플러그인 친화적인 구조(Mover-Native)로 완전히 설계를 변경**하여 버그를 완벽히 격리했습니다.

1. **지연 바인딩(Lazy Binding) 도입**: `TickComponent` 내부에 방어 코드를 추가하여, `ExRunnerInputComponent`가 뒤늦게 들어오더라도 즉시 감지하여 델리게이트를 성공적으로 체결하도록 수정.
2. **블루프린트 종속 끊기**: 기존 컨트롤러에 무식하게 덧셈 연산을 가하던 `Add Controller Yaw Input` 노드를 삭제.
3. **Mover 네이티브 조향 적용**:
    - `UpdateCharacterRotation(Tick)` 에 있던 지저분하고 충돌을 유발하는 `RInterpTo` 로직 완전 삭제.
    - `bUseControllerRotationYaw = false` 로 변경하여 엔진 레벨의 강제 고정 해방.
    - `ProduceInput_Implementation` 안에서 계산된 부드러운 타겟 경로(Path Forward + TargetLookYawOffset)를 `OrientationIntent`와 `DirectionalInput`에 직접 주입.
4. **GameModeDataSet 널(Null) 체크 보강**: 
    - 향후 BP 디테일 패널에서 데이터 에셋이 연결되지 않아 발생하는 소리없는 실패(Silent Failure)를 막기 위해, Null일 경우 기본값 `45.0f`를 강제하고 `Warning` 로그를 띄우는 방어 코드를 복구.
    - 이와 동일한 설계 철학을 `ExFrameWork_Guidelines.md` 에 "1.7 언리얼 엔진 베스트 프랙티스 - 검증(Assertions) 및 필수 포인터 체크" 조항으로 신설 기록 완료.
