# [Bug] Mover 플러그인 연동 시 캐릭터 회전 무시 현상

## 현상 (Issue)
- 커브 구간에서 캐릭터의 궤도(경로 방향) 회전이 의도대로 부드럽게 이루어지지 않는 문제가 발생.
- `AExRunnerGameMode`에서 캐릭터의 컨트롤러 방향(`ControlRotation`)을 강제로 경로의 접선 방향으로 보정하고, 폰의 `bUseControllerRotationYaw = true`를 통해 캐릭터를 회전시키고자 함.
- 그러나 `UExRunnerMovementComponent` (Mover InputProducer 기능)를 캐릭터에 부착하면, 캐릭터 모델의 회전이 일어나지 않고 뻣뻣하게 직선을 바라보는 현상이 발생. (해당 컴포넌트를 떼면 자연스럽게 회전함)

## 원인 (Cause)
- Mover 컴포넌트는 내부 시뮬레이션 과정에서 이동 방향(`DirectionalIntent`)과 회전 방향(`OrientationIntent`) 입력을 기반으로 자체 구조 내에서 동기화 State를 업데이트하고 캐릭터 모델을 다시 정렬(Orientation)하는 기능이 있음.
- 기존 `UExRunnerMovementComponent::ProduceInput_Implementation`에서는 매 틱 Mover 시스템에 전진하라는 이동 의도(`Inputs->SetMoveInput(EMoveInputType::DirectionalIntent, ForwardDir);`)만을 주고 있었음.
- 이로 인해 Mover 엔진은 회전 의도(`OrientationIntent`)가 비어 있는 상태 또는 기존 입력 데이터로 처리하면서, GameMode에서 설정한 `ControlRotation` 기반의 캐릭터 회전을 덮어써버리거나 무시(충돌)하는 상황이 발생함.

## 해결 (Resolution)
- `UExRunnerMovementComponent::ProduceInput_Implementation` 내부 로직에 시뮬레이션 의도를 명확하게 추가 세팅함.
- 이동 방향(`ForwardDir`)과 동일하게 캐릭터 모델도 해당 방향을 바라봐야 하므로, Mover의 회전 의도 타겟인 `OrientationIntent` 필드에 `ForwardDir`를 명시적으로 주입.

```cpp
		// DirectionalIntent로 이동 입력 설정 (크기 1.0)
		Inputs->SetMoveInput(EMoveInputType::DirectionalIntent, ForwardDir);

		// [Fix] Mover 시스템이 캐릭터의 모델(Mesh) 방향도 컨트롤러가 바라보는 방향(경로 접선)으로 맞춰 회전시킬 수 있도록 OrientationIntent를 주입.
		Inputs->OrientationIntent = ForwardDir;
```

## 키워드 (Keywords)
Mover, InputProducer, OrientationIntent, Rotation, Curve, UExRunnerMovementComponent, bUseControllerRotationYaw
