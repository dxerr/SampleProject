# SandboxCharacter_CMC 분석 및 모델 교체 시스템 가이드 (Version 2)

사용자 피드백("하위 액터로 붙여서 교체하는 시스템")에 따라 심층 분석을 수행한 결과, 캐릭터 교체 로직의 핵심이 **Game Mode (`GM_Sandbox`)**에 있음을 확인했습니다.

## 1. 핵심 분석 결과: Game Mode가 컨트롤 타워

초기 분석에서는 `SandboxCharacter_CMC` 자체를 의심했으나, 의존성 분석 결과 해당 블루프린트는 독립적이며 다른 캐릭터(Twinblast 등)를 참조하지 않습니다. 반면, `GM_Sandbox`는 이 모든 캐릭터를 참조하고 있습니다.

### 1.1 구조 다이어그램
```mermaid
graph TD
    GM[GM_Sandbox (Game Mode)] -->|1. Spawns| CMC[SandboxCharacter_CMC (Default Pawn)]
    GM -->|2. References| Twin[BP_Twinblast]
    GM -->|2. References| Echo[BP_Echo]
    GM -->|2. References| Manny[BP_Manny]
    
    GM -->|3. Spawns & Attaches| Avatar[Child Actor / Avatar]
    Avatar -.->|Attached To| CMC
```

### 1.2 상세 증거 (Python Bridge Analysis)
*   **SandboxCharacter_CMC**: `BP_Twinblast`, `BP_Echo` 등에 대한 참조(Reference)가 **전혀 없습니다.** 즉, 스스로 모델을 교체하는 로직을 가지고 있지 않습니다.
*   **GM_Sandbox**:
    *   **DefaultPawnClass**: `SandboxCharacter_CMC`로 설정됨 (기본 플레이어).
    *   **Dependencies**: `BP_Twinblast`, `BP_Echo`, `BP_Manny`, `BP_Quinn`, `BP_UE4_Mannequin` 등을 모두 참조하고 있습니다.

## 2. 교체 로직의 위치 및 작동 방식

사용자가 찾고 있는 **"교체하는 부분"**은 `SandboxCharacter_CMC` 내부가 아니라 **`GM_Sandbox` 블루프린트의 Event Graph**에 구현되어 있을 확률이 매우 높습니다.

### 예상되는 작동 흐름 (Flow)
1.  게임 시작 시 `GM_Sandbox`가 기본 폰인 `SandboxCharacter_CMC`를 스폰합니다.
2.  `GM_Sandbox`의 **OnPostLogin** 또는 **BeginPlay** 이벤트에서:
    *   특정 조건(변수, 랜덤 등)에 따라 `BP_Twinblast` 클래스를 선택합니다.
    *   **SpawnActor** 노드를 통해 해당 캐릭터를 생성합니다.
    *   **AttachToActor** (또는 AttachToComponent) 노드를 사용하여 생성된 캐릭터를 `SandboxCharacter_CMC`에 부착합니다.
    *   이때 `SandboxCharacter_CMC`의 원래 Mesh를 **SetHiddenInGame(True)** 처리하여 숨깁니다.

## 3. 수정 및 분석 가이드

모델 교체 방식을 변경하거나 분석하려면 다음 위치를 확인하세요.

1.  **`GM_Sandbox` 블루프린트 열기**: `/Game/Blueprints/GM_Sandbox`
2.  **Event Graph 호근 함수 확인**:
    *   `SpawnActor`, `Attach`, `ChildActor` 관련 노드를 검색하세요.
    *   캐릭터 클래스 배열(Array) 변수가 있는지 확인하세요.
3.  **`AC_VisualOverrideManager` 확인**:
    *   `GM_Sandbox`가 이 컴포넌트를 사용하여 로직을 위임했을 수도 있습니다.

## 4. 요약

> "SandboxCharacter_CMC 스켈레탈메시는 Hide처리를 하는것 같은데 이 교체 하는 부분이 어딘지 모르겠어"

**정답**: 그 교체 로직은 **`GM_Sandbox` (Game Mode)**에 있습니다. `SandboxCharacter_CMC`는 껍데기(Container) 역할만 수행하며, 실제 알맹이(Twinblast 등)는 Game Mode가 넣어주고 있는 구조입니다.
