# SandboxCharacter_CMC 컴포넌트 상세 분석 보고서

`/Game/Blueprints/SandboxCharacter_CMC` 블루프린트 클래스를 구성하는 컴포넌트들에 대한 정밀 분석 결과입니다. 이 캐릭터는 언리얼 엔진의 기본 `Character` 클래스를 상속받으며, Motion Matching 및 파쿠르(Traversal) 시스템을 위한 다수의 커스텀 컴포넌트를 포함하고 있습니다.

## 1. 핵심 컴포넌트 구성 요약
이 캐릭터는 크게 **기본 이동/렌더링**, **카메라 시스템**, **특수 기능(커스텀)** 세 가지 그룹의 컴포넌트로 구성됩니다.

### A. 기본 컴포넌트 (Native Inherited)
기본 `ACharacter` 클래스에서 상속받은 필수 컴포넌트들입니다.
*   **CollisionCylinder (CapsuleComponent)**: 캐릭터의 물리적 충돌을 담당하는 캡슐입니다. 루트 컴포넌트입니다.
*   **CharacterMesh0 (SkeletalMeshComponent)**: `SKM_UEFN_Mannequin`을 사용하는 메인 시각적 메시입니다.
*   **CharMoveComp (CharacterMovementComponent)**: 이동 로직(걷기, 점프, 낙하 등)을 처리하는 핵심 이동 컴포넌트입니다. `CMC`라는 이름이 붙은 이유이기도 합니다.
*   **Arrow (ArrowComponent)**: 에디터에서 캐릭터의 정면을 표시하는 디버그용 화살표입니다.

### B. 카메라 및 시점 관련
*   **SpringArm (SpringArmComponent)**: 카메라가 캐릭터를 따라다니게 하며, 벽 뚫림을 방지합니다.
*   **GameplayCamera (GameplayCameraComponent)**: (추정) 새로운 게임플레이 카메라 시스템을 위한 컴포넌트로 보입니다.
*   **Camera(NotUsedByDefault) (CameraComponent)**: 기본적으로는 사용되지 않으나, 필요한 경우 활성화할 수 있는 예비용 카메라입니다.

### C. 커스텀 기능 컴포넌트 (Custom Logic)
이 프로젝트의 핵심인 Motion Matching과 상호작용을 구현하기 위해 추가된 블루프린트 컴포넌트들입니다.

| 컴포넌트 이름 | 클래스 (Blueprint) | 기능 및 역할 분석 |
| :--- | :--- | :--- |
| **AC_PreCMCTick** | `AC_PreCMCTick_C` | **사전 이동 계산**: Character Movement Component(CMC)가 틱(Tick)을 돌기 *이전에* 처리해야 할 로직을 담고 있습니다. 예를 들어, 입력 보정이나 물리 상태 초기화 등이 여기서 수행될 수 있습니다. |
| **AC_FoleyEvents** | `AC_FoleyEvents_C` | **효과음(Foley) 관리**: 발자국 소리, 옷 스치는 소리 등 캐릭터 움직임에 따른 사운드 이벤트를 처리합니다. 애니메이션 노티파이와 연동될 가능성이 높습니다. |
| **AC_TraversalLogic** | `AC_TraversalLogic_C` | **파쿠르/장애물 극복**: Motion Matching의 핵심인 'Traversal'(담넘기, 오르기 등) 로직을 전담합니다. 장애물 감지(Trace) 및 적절한 모션 매칭 쿼리를 발생시키는 역할을 합니다. |
| **BP_VisualOverrideManager** | `AC_VisualOverrideManager_C` | **외형 교체 관리**: `GM_Sandbox`와 연동되어 캐릭터의 스킨이나 장비를 교체하는 기능을 담당할 것으로 보입니다. `DDCvar` 설정에 따른 비주얼 변경 신호를 처리할 수 있습니다. |
| **AC_SmartObjectAnimation** | `AC_SmartObjectAnimation_C` | **스마트 오브젝트 연동**: 게임 월드 내의 상호작용 가능한 물체(Smart Object)와 캐릭터 애니메이션을 동기화합니다. (예: 의자에 앉기, 문 열기 모션 등) |

### D. 기타
*   **자손 액터 컴포넌트 (ChildActorComponent)**: 현재 `Child Actor Class`가 **None**으로 설정되어 있습니다. 이는 런타임에 동적으로 설정되거나, 혹은 현재는 더미(Placeholder)로 남아있는 상태입니다. (이전 분석에서 `GM_Sandbox`가 별도의 캐릭터를 스폰하여 붙이는 방식임이 확인되었으므로, 이 컴포넌트는 사용되지 않거나 보조적인 용도일 수 있습니다.)
*   **MotionWarping (MotionWarpingComponent)**: 애니메이션의 루트 모션을 늘리거나 줄여서 목표 지점(예: 파쿠르 착지점)에 정확히 맞추는 **모션 워핑** 기술을 지원합니다. `AC_TraversalLogic`과 밀접하게 연동됩니다.

## 2. 결론 및 참고사항
`SandboxCharacter_CMC`는 단순히 로직을 담는통(Container) 역할을 하며, 실제 복잡한 기능(파쿠르, 사운드, 외형)은 각각의 **AC_ (Actor Component)** 들로 모듈화되어 분리되어 있습니다.

*   **기능 수정 시**: 파쿠르 로직을 고치려면 `AC_TraversalLogic`을, 사운드를 고치려면 `AC_FoleyEvents`를 열어야 합니다.
*   **외형 관련**: `BP_VisualOverrideManager`가 존재하므로, 모델 교체 로직의 세부 구현은 `GM_Sandbox`뿐만 아니라 이 컴포넌트 내부도 확인해볼 가치가 있습니다.

이 구조는 유지보수가 용이하도록 기능을 철저히 분리(Component-Based Design)한 모범적인 사례입니다.
