# [Movement] Controller Rotation Conflict (Jittering) 통합 리포트

## 1. 개요
언리얼 Mover 플러그인을 사용하여 캐릭터 회전을 처리할 때, 기존 컨트롤러(Yaw) 회전 로직과 Mover의 `OrientationIntent`가 충돌하여 발생하는 화면 떨림(Jittering) 및 회전 무시 현상을 해결했습니다.

## 2. 주요 문제 및 원인 분석

### A. 회전 지터링 (`Jittering`)
*   **증상**: 캐릭터가 회전할 때 좌우로 미세하게 떨리거나 특정 각도에서 고정됨.
*   **원인**: `GameMode`의 틱에서 수동으로 `SetActorRotation`을 호출하는 것과, Mover 플러그인의 `UpdatePosition` 단계에서 회전을 계산하는 것이 서로 다른 프레임 데이터로 보간을 시도함 (Dirty State 충돌).

### B. 회전 의도 무시 (`Missing OrientationIntent`)
*   **증상**: 조이스틱을 입력해도 캐릭터 모델이 정면만 바라봄.
*   **원인**: `UExRunnerMovementComponent::ProduceInput`에서 `OutInput.OrientationIntent`를 명시적으로 채워주지 않아, Mover 엔진이 '회전 의지가 없음'으로 판단하고 강제로 정면 고정함.

## 3. 해결 방안
1.  **회전 로직 단일화**: `GameMode`의 수동 회전 제어를 제거하고, 모든 회전 요청은 `OrientationIntent`를 통해 Mover 플러그인에 '부탁'하는 구조로 변경.
2.  **OrientationIntent 주입**:
    ```cpp
    // InputProducer에서 최종 목표 방향을 계산하여 주입
    OutInput.OrientationIntent = TargetLookAtRotation.Vector();
    ```
3.  **틱 순서 제어**: `MoverComponent`가 입력을 처리한 후 비주얼 메시가 따라오도록 `PostPhysics` 단계로 이동.

## 4. 최종 결과
이제 캐릭터는 어떠한 환경에서도 떨림 없이 매끄럽게 회전하며, Mover 플러그인의 표준 리플리케이션 시스템과 완벽히 호환됩니다.
