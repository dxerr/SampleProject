# 🧗 World Shift 시스템에서의 Climb 애니메이션 동기화 가이드

**문제 상황**: 
`AExFloorChunk`(바닥)가 `ShiftWorld`에 의해 뒤로 밀릴 때, 캐릭터의 Climb 애니메이션(Root Motion)이 허공을 잡거나 위치가 어긋나는 현상 발생.

**원인**: 
일반적인 Motion Warping은 타겟 위치를 **고정 좌표(Static Location)**로 계산하는 경우가 많습니다. World Shift로 인해 타겟(장애물)이 이동하면, 이미 계산된 워핑 포인트와 실제 오브젝트 위치 사이에 괴리가 생깁니다.

## ✅ 해결 방법: "Moving Target" 추적 설정

Motion Warping이 이동하는 장애물을 실시간으로 따라가도록 설정해야 합니다.

### 1. Motion Warping 설정 변경 (AnimBP / CharacterBP)

이동하는 물체에 대해 워핑할 때는 `Add or Update Warp Target` 노드를 사용해야 하며, 반드시 **컴포넌트 추적(Follow Component)** 옵션을 켜야 합니다.

1.  **Motion Warping Notify State 확인**:
    *   Climb 몽타주(Animation Montage) 안에 있는 `MotionWarping` Notify State를 확인합니다.
    *   `Warp Target Name`이 무엇인지 확인합니다 (예: `ClimbPoint`, `Ledge`).

2.  **Blueprint 로직 수정 (Climb 진입 시점)**:
    *   장애물의 **Climb용 SceneComponent(소켓)**나 **Actor 자체**를 참조합니다.
    *   `Add or Update Warp Target from Component` 노드를 호출합니다.
    *   **중요 설정**:
        *   `Warp Target Name`: 몽타주의 이름과 일치 (예: `ClimbPoint`)
        *   `Component`: 장애물의 루트 컴포넌트나 잡을 위치(SceneComp) 연결
        *   `Bone Name`: (Optional) 
        *   `bFollow Component`: **TRUE (체크 필수!)** ☑️

> **`bFollow Component = True`**로 설정하면, Motion Warping 시스템이 매 프레임 타겟 컴포넌트의 위치 변화를 감지하여 워핑 궤적을 실시간으로 수정합니다. 트레드밀 시스템에서 장애물이 뒤로 밀려나도 캐릭터가 끝까지 달라붙게 됩니다.

### 2. 예외 처리 (Tick 순서)

만약 위 설정으로도 미세한 떨림이 있다면, **Tick Group** 문제일 수 있습니다.
`ExRunnerMovementComponent` (World Shift 발생)와 `MotionWarpingComponent` (보정 발생) 간의 순서가 중요합니다.

*   `ExRunnerMovementComponent`의 Tick Group을 `PrePhysics`로 설정 (이미 기본값).
*   `MotionWarpingComponent`는 보통 `PostPhysics`나 `DuringPhysics`에서 동작하므로 자연스럽게 해결될 것입니다. (만약 떨린다면 GameMode 스피드 로직을 확인해보세요)

---
### 요약
장애물(Generic Actor)은 `ExFloorChunk`에 붙어서 계속 이동 중입니다.
따라서 **Motion Warping**에게 "고정된 좌표가 아니라, **저 컴포넌트를 계속 따라가라(Follow)!**"고 지시해야 합니다.
