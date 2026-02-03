# 러너 캐릭터 (`ExRunnerCharacter`) 구현 상세 계획 (Rev. 2)

**작성일**: 2026-02-02
**목표**: `AnimationSample`의 `SandboxCharacter` 기능을 활용하며, **곡선 및 커브 트랙**에서도 자연스럽게 주행할 수 있는 캐릭터 구현.

---

## 1. 개요 및 설계 철학
*   **기반 클래스**: `ACharacter` (CMC - `UCharacterMovementComponent` 활용).
    *   `SandboxCharacter_CMC`와 유사한 구조를 갖추되, 러너 게임에 특화된 로직 추가.
*   **위치**: `ExCore/Source/ExCoreRuntime/Actors/AExRunnerCharacter`
*   **핵심 철학**:
    *   **Forward-Oriented**: 절대 좌표계(World X축)가 아닌, **캐릭터의 현재 전방 벡터(Actor Forward Vector)**를 기준으로 이동한다.
    *   **Relative Lane System**: 레인 이동 역시 **우측 벡터(Actor Right Vector)**를 기준으로 계산하여, 트랙이 휘어져도 레인 간격을 유지한다.

---

## 2. 이동 메커니즘 (Curve-Ready Movement)

### 2.1 전방 주행 (Forward Running)
트랙이 휘어지거나 회전해도 캐릭터는 항상 "자신의 앞"으로 달립니다.

*   **구현 로직 (`Tick`)**:
    *   매 프레임 `GetActorForwardVector()`를 가져옵니다.
    *   `AddMovementInput(ForwardVector, 1.0f)`를 호출하여 CMC에 이동 명령을 전달합니다.
    *   **속도 제어**: `ExCoreGameMode`의 `CurrentGameSpeed`를 `GetCharacterMovement()->MaxWalkSpeed`에 동기화합니다.
*   **커브 대응**:
    *   캐릭터의 회전(Yaw)은 바닥의 Spline이나 기울기에 맞춰 지속적으로 조정되어야 합니다. (추후 구현될 `MF_ExCurvedWorld` 또는 로직에 따름).
    *   우선 초기 단계에서는 **바닥의 회전축**이나 **Input**에 따라 캐릭터가 회전하면, 이동 로직은 자동으로 그 방향을 따릅니다.

### 2.2 레인 시스템 (Relative Lane System)
좌우 이동 역시 월드 좌표가 아닌 로컬 좌표계를 따릅니다.

*   **변수**:
    *   `CurrentLaneOffset` (float): 현재 중심으로부터의 떨어진 거리 (예: -300, 0, 300).
    *   `TargetLaneIndex` (int32): 목표 레인 (-1, 0, 1).
    *   `LaneWidth` (float): 레인 폭.
*   **이동 로직**:
    *   입력(Left/Right) 시 `TargetLaneIndex` 변경.
    *   목표 오프셋 = `TargetLaneIndex * LaneWidth`.
    *   `CurrentLaneOffset`을 `FMath::FInterpTo`로 부드럽게 갱신.
    *   **위치 보정**:
        *   `NewLocation = CurrentLocation + (RightVector * (DeltaOffset))`
        *   즉, 이전 프레임 대비 변화량만큼을 **현재의 Right Vector** 방향으로 이동시킵니다.

### 2.3 점프 (Jump)
*   `ACharacter::Jump()` 기본 기능 사용.
*   `SandboxCharacter`의 CMC 설정을 참고하여 `JumpZVelocity`, `AirControl` 등을 조정하여 쾌적한 조작감 확보.

---

## 3. 기능 및 컴포넌트 활용 (Sandbox 스타일)

### 3.1 CMC (Character Movement Component)
*   `SandboxCharacter_CMC` 블루프린트의 설정을 최대한 모사합니다.
    *   `GravityScale`: 점프 체공 시간 조절.
    *   `BrakingDecelerationWalking`: 급정거/방향전환 반응성.
    *   `AirControl`: 점프 중 레인 변경 허용 여부 제어.

### 3.2 카메라 (Camera) - 최소화
*   사용자 요청에 따라 **복잡한 카메라 모드 구현은 배제**합니다.
*   `SandboxCharacter`에 내장된 카메라 시스템이나 추후 도입될 시스템과의 호환성을 위해, 본 클래스에서는 **가장 기본적인 `SpringArm` + `Camera`** 만을 테스트 목적으로 부착하거나, 아예 생략하고 외부 카메라 매니저에 의존할 수도 있습니다.
*   **결정**: 초기 테스트 및 디버깅을 위해 `USpringArmComponent` 하나만 최소한으로 부착(VisibleAnywhere)하되, 로직 간섭은 없도록 합니다.

---

## 4. 입력 처리 (Input)
*   **Enhanced Input** 시스템 사용.
*   **Action**: `IA_Move(Axis1D)`, `IA_Jump`.
*   **로직**:
    *   `IA_Move` (A/D): 값이 들어오면 즉시 레인 변경 트리거. (누르고 있는 동안 이동이 아니라, 탭 하면 변경).

---

## 5. 작업 순서
1.  **C++ 클래스 작성 (`AExRunnerCharacter`)**: `ACharacter` 상속.
2.  **이동 로직 구현**: `Forward/Right Vector` 기반의 `Tick` 업데이트.
3.  **블루프린트 생성 (`BP_ExRunnerCharacter`)**: `ExCoreGameMode`에 Default Pawn으로 등록.
4.  **CMC 튜닝**: `SandboxCharacter` 느낌이 나도록 파라미터 조정.
