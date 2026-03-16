# 러너 플레이 Yaw 입력 곡선 경로 연동 및 보간 수정 (V2)

## 현상파악
- 이전 수정에서 `ExRunnerInputComponent`가 컨트롤러의 Yaw 각도를 직접 덮어쓰고 제어하도록 만들었으나, 곡선 맵 등에서(스플라인 경로를 따라가야 하는 경우) 캐릭터가 월드 회전을 타지 못하는 문제가 발생했습니다.
- 원인은 `ExRunnerMovementComponent` 내부에서 경로를 따라 캐릭터의 정면을 잡아주던 `UpdateCharacterRotation` 로직이 비활성화되었고, 강제로 최초 회전각(`BaseYaw`)을 0점으로 잡도록 설계했기 때문이었습니다. 이로 인해 스플라인 트랙이 구부러져도 캐릭터가 올바른 방향을 전혀 바라보지 못했습니다.

## 조치 내용
**"스플라인 경로 기준 이동 + 조이스틱 추가 회전(오프셋)"** 방식으로 각 컴포넌트의 역할을 완전히 분리하여 재설계했습니다.

1. **`ExRunnerMovementComponent` (경로 추적 복구 및 오프셋 합산)**
   - 비활성화했던 `UpdateCharacterRotation(DeltaTime)`을 다시 활성화했습니다. 이제 캐릭터는 곡선 길(스플라인의 접선 방향)에 맞춰 자동으로 매끄럽게 정면을 바라보며 나아갑니다.
   - 이때 바라볼 타겟 각도(`TargetRot.Yaw`)에, 입력 컴포넌트가 계산해 둔 **조이스틱 좌우 오프셋 각도**(`InputComp->GetCurrentYawOffset()`)를 매 프레임 찾아내어 합산(Add)토록 설계했습니다.

2. **`ExRunnerInputComponent` (오프셋 보간 전담)**
   - 이제 컨트롤러 회전을 직접 조작(`AddControllerYawInput`)하지 않습니다.
   - 스와이프 X축 값(-1.0 ~ 1.0)을 받으면, `MaxRunnerYawAngle`을 한계치로 삼아 **목표 오프셋 각도**(`TargetYawOffset`)만 산출합니다.
   - `TickComponent`에서 이 목표 오프셋과 현재 오프셋(`CurrentYawOffset`)을 `FInterpTo`로 부드럽게 보간(속도값 = `RunnerLookSensitivity`)하며, 이 보간된 상태 스냅샷을 `GetCurrentYawOffset()`으로 MovementComponent에 제공합니다.
   - 입력이 중단될 시(`bIsLookRequested == false`), 목표 오프셋은 즉시 `0.0f`가 되어 캐릭터 시선이 경로의 진짜 정면으로 아주 자연스럽게 스르륵 복귀합니다.
