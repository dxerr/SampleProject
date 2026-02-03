# 러너 무브먼트 컴포넌트 (`ExRunnerMovementComponent`) 구현 계획

**작성일**: 2026-02-02
**목표**: `AExRunnerCharacter` (Actor) 방식을 폐기하고, 기존 시스템과 호환되는 **컴포넌트(Component) 방식**으로 러너 로직(AutoRun, Lane Change)을 구현.

---

## 1. 설계 개념 (Design Concept)
*   **역할 분담**:
    *   **SandboxCharacter_Mover (BP)**: 물리 이동(CMC), 입력 수신, 애니메이션 실행 주체. (Container Pawn)
    *   **VisualOverride Actor (BP)**: 캐릭터의 외형 담당. (Mover에 Attach됨)
    *   **ExRunnerMovementComponent (C++)**: **Visual Actor에 부착**되어, 상위 **Mover Character를 강제로 전진**시키고 레인 변경을 제어하는 두뇌 역할.
*   **장점**: 
    *   기존 `SpawnDataAsset` 시스템을 그대로 활용 가능.
    *   Mover BP를 수정하지 않고도, Visual BP에 컴포넌트만 달면 러너 기능이 활성화됨.
    *   다양한 Visual마다 다른 러너 설정을 가질 수도 있음.

## 2. 클래스 명세
*   **클래스명**: `UExRunnerMovementComponent`
*   **부모 클래스**: `UActorComponent`
*   **위치**: `ExCore/Source/ExCoreRuntime/Components/`

### 2.1 주요 기능
1.  **Target Pawn 감지 (`BeginPlay`)**:
    *   `GetOwner()`는 Visual Actor입니다.
    *   `GetOwner()->GetAttachParentActor()`를 통해 **실제 조종해야 할 Character (Mover)**를 찾습니다.
    *   찾은 Character의 `CharacterMovementComponent`를 캐싱합니다.

2.  **Auto Run (`TickComponent`)**:
    *   `ExCoreGameMode`의 `CurrentGameSpeed`를 가져옵니다.
    *   Target Character에게 매 프레임 `AddMovementInput(GetActorForwardVector())`를 호출합니다.
    *   Target Character의 `MaxWalkSpeed`를 게임 속도에 맞춥니다.

3.  **Lane System (횡이동)**:
    *   `CurrentLaneIndex` (-1, 0, 1) 관리.
    *   Target Character의 위치를 `Right Vector` 기준으로 보간(`VInterp`)하여 이동시킵니다.
    *   입력 처리:
        *   직접 입력을 받지 않고, **Blueprint Callable 함수 (`MoveLeft`, `MoveRight`)**를 제공합니다.
        *   Mover BP의 `IA_Move` 이벤트에서 이 함수를 호출하도록 연결합니다.

## 3. 구현 상세 로직

### 3.1 초기화
```cpp
void BeginPlay() {
    AActor* VisualActor = GetOwner();
    TargetCharacter = Cast<ACharacter>(VisualActor->GetAttachParentActor());
    if (!TargetCharacter) {
        // 혹시 Visual이 Root에 안 붙고 Mover가 직접 이 컴포넌트를 가질 수도 있음
        TargetCharacter = Cast<ACharacter>(VisualActor);
    }
}
```

### 3.2 틱 로직
```cpp
void TickComponent(...) {
    if (!TargetCharacter) return;
    
    // 1. 속도 동기화 (GameMode -> Character)
    
    // 2. 전방 이동 (Curve 대응: Forward Vector 사용)
    TargetCharacter->AddMovementInput(TargetCharacter->GetActorForwardVector());
    
    // 3. 레인 변경 (Position Interp)
    UpdateLanePosition(DeltaTime);
}
```

## 4. 작업 순서
1.  **기존 파일 삭제**: `ExRunnerCharacter.h`, `ExRunnerCharacter.cpp`.
2.  **새 클래스 생성**: `UExRunnerMovementComponent`.
3.  **로직 이식**: 기존 Character에 있던 AutoRun/LaneChange 로직을 컴포넌트로 이동.
4.  **BP 통합 가이드**: 사용자가 Mover BP에서 이 컴포넌트 함수를 호출하는 방법 안내.
