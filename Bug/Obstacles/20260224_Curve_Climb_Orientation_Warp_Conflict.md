# Bug Report: 커브 구간 등반(Climb) 실패 현상 분석

## 이슈 개요
- **증상**: 스크린샷의 디버그 라인 트레이스 결과(`Has Front Ledge: true`, `Has Back Ledge: true` 등)를 보면 등반 감지 로직 자체는 성공적으로 장애물 위를 찾았습니다. 하지만, 실제 캐릭터가 점프(Climb) 동작을 제대로 수행하지 못하고 장애물을 넘지 못하는 현상이 발생합니다. 특히 **커브 구간**에 배치된 장애물에서 두드러집니다.

## 원인 분석 (Root Cause)

1. **`ExRunnerGameMode::Tick` 내의 강제 회전 덮어쓰기 로직의 충돌**
   - V2 브랜치 리팩토링 과정에서 기존의 `OnClimbStart`, `OnClimbEnd` 이벤트 콜백(트레드밀 속도 및 회전 영향을 중지하던 기능)이 삭제되었습니다.
   - 현재 `ExRunnerGameMode::Tick`은 매 프레임 무조건적으로 `UpdateCharacterRotation(DeltaTime)`을 호출하여, 캐릭터의 `ActorRotation.Yaw` 값을 경로 진행 방향(`LookAheadDist` 접선 + 로컬 스티어링 오프셋)으로 강제 고정시킵니다.

2. **직선과 커브 구간의 차이점**
   - **직선 구간**: 경로의 접선 방향과 장애물의 면 방향(Normal)이 완벽히 일치합니다. 따라서 몽타주의 Motion Warping/Root Motion이 요구하는 회전값과 GameMode가 강제로 맞추는 회전값이 동일하여 등반이 정상 작동합니다.
   - **커브 구간 (현재 문제의 핵심)**: 
     - 커브 위에 배치된 스폰된 장애물은 곡선의 **현(Chord)**을 따라 정렬됩니다. 반면 `ExRunnerGameMode`가 캐릭터에게 바라보라고 지시하는 방향은 곡석의 조금 앞쪽(`LookAheadDist`)의 **접선(Tangent)** 방향입니다.
     - 등반 동작 진입 시점(Root Motion 구간)에 Motion Warping 시스템은 캐릭터를 장애물 면에 정면으로 수직 지향(Align)시키려고 회전을 시도하지만, **동시에 `AExRunnerGameMode::Tick`은 매 프레임 캐릭터를 경로 회전 반경 안쪽으로 강제로 비틀어버립니다(Yaw 덮어쓰기).**
     - 결국 캐릭터의 캡슐이 회전 상호 충돌로 인해 장애물 벽에 비스듬히(사선으로) 파고들거나 막혀버리고, Warp Target(위치)으로 도달할 Root Motion 궤도를 벗어나게 되어 등반 애니메이션이 떨어지게 됩니다.

## 해결 방안 (Resolution)

장애물을 넘는(Traversal) 동안에는 경로 추종 회전 연산을 잠시 멈추고, Motion Warping과 애니메이션에 캐릭터의 제어권(회전 권한)을 온전히 넘겨줘야 합니다.

1. **상태 감지 추가 (ExRunnerGameMode.cpp)**
   - V1에 있었던 `OnClimbStart`, `OnClimbEnd` 등의 이벤트를 복원하여 `bIsClimbing`과 같은 상태 플래그를 변수로 관리. 또는 `GameplayTag` 로 확인.
   
2. **`UpdateCharacterRotation` 조건부 적용 처리**
   - 등반 중이거나 공중에 있을 때(또는 특정 모션 중일 때)는 캐릭터 캡슐 자체의 Yaw 회전 방향이 커브 접선으로 돌아가지 않도록 보호하여, 장애물 정면과 맞닿아 자연스럽게 벽을 타고 오르도록 돕습니다.

## 결론
디버깅 테스트 결과로 보아, 장애물의 전방 Ledge 판별과 트레이스는 정상입니다. 근본적인 원인은 V2 리팩토링 후 삭제된 **`GameMode`의 등반(Climb) 예외 처리**이며, 루트 모션과 GameMode Tick 간의 강제 시선 방향 **"회전 주도권 충돌" (Tug-of-war)**로 인해 커브 구간 발생 각도 차이에서 모션이 꼬여 밀어내기 되는 현상입니다.
