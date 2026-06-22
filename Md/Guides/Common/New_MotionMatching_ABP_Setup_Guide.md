# Motion Matching ABP 구축 & 분석 가이드 (통합본)

> 2026-06-22: 구 `MotionMatching_Guide_KR.md`(분석/개념 중심)와 본 문서(From Scratch 구축)를 하나로 통합했습니다.

**"바닥부터 새로(From Scratch)"** Motion Matching 시스템을 구축하는 상세 절차와, 그 기반이 되는 개념·디버깅 방법을 함께 다룹니다.

> ⚠️ **에셋 경로 주의:** 본 가이드에서 인용하는 `/Game/Characters/UEFN_Mannequin/Animations/MotionMatchingData/...`, `PSS_Default_Mover`, `CHT_PoseSearchDatabases_Mover`, `SandboxCharacter_Mover_ABP` 등은 **Epic의 Game Animation Sample 프로젝트** 에셋입니다. **ExFrameWork 본 프로젝트에는 존재하지 않으며**, 참고/스키마 재사용 예시로만 사용하세요. 본 프로젝트용 신규 에셋은 `Content/` 하위에 직접 생성합니다.

## 전제 조건
1.  **Character Blueprint**: `CharacterMovementComponent`가 있는 기본 캐릭터가 있어야 합니다.
2.  **Trajectory Component**: 캐릭터 블루프린트에 `CharacterTrajectoryComponent`가 추가되어 있어야 합니다. (모션 매칭의 필수 입력값)

---

## 0. 개념: Chooser Table 기반 구조란?

- **기존 방식**: AnimGraph에서 하나의 `Pose Search Database`(PSD)를 고정해서 사용.
- **현재 방식 (Chooser)**: AnimGraph는 **Chooser Table(CHT)** 을 바라본다. 이 테이블이 게임플레이 태그·속도·무기 상태 등을 판단하여 *"지금은 걷기 DB를 써라", "지금은 점프 DB를 써라"* 라고 동적으로 PSD를 결정해 준다.
- 모션 매칭은 매 프레임 수천 개 포즈 중 현재 속도·회전·발 위치 등이 가장 일치하는 프레임을 찾아 블렌딩한다. 검색의 핵심 입력은 캐릭터의 **Trajectory(과거 경로 + 미래 예측 경로)** 다.

---

## 1. 데이터 에셋 준비 (Chooser & Database)

가장 먼저 엔진이 "무엇을 검색할지" 알려주는 데이터를 준비해야 합니다.

### 1-1. Pose Search Database (PSD) 만들기
1.  **Pose Search Schema** 준비: `/Game/Characters/UEFN_Mannequin/Animations/MotionMatchingData/Schemas/PSS_Default_Mover` (기존 스키마 활용 권장)
2.  **PSD 생성**: `Content/MyGame/MotionMatching` 폴더에 `PSD_Locomotion` 생성.
3.  **애니메이션 등록**: 걷기, 뛰기, 멈춤 등의 `Sequence`나 `BlendSpace`를 등록합니다.

### 1-2. Chooser Table (CHT) 만들기 (핵심)
단일 DB를 직접 쓰는 것보다, 확장을 위해 테이블을 거치는 것이 **Best Practice**입니다.
1.  **Chooser Table 생성**: `CHT_Locomotion` 생성.
2.  **Output Type 설정**: 디테일 패널에서 Output Object Type을 `PoseSearchDatabase`로 설정.
3.  **행(Row) 추가**:
    *   기본 행(Default Row)을 추가하고 `Result`에 `PSD_Locomotion`을 넣습니다.
    *   (옵션) 추후 `Dashboard` 태그가 있으면 `PSD_Sprinting`을 쓰도록 로직 추가 가능.

---

## 2. Animation Blueprint (ABP) 생성 및 설정

### 2-1. ABP 생성
1.  `AnimInstance`를 부모로 하는 `ABP_Review_Mover` 생성.
2.  Target Skeleton: `UEFN_Mannequin_Skeleton` (또는 사용 중인 스켈레톤).

### 2-2. [Event Graph] Trajectory 업데이트 로직
모션 매칭 노드는 'Trajectory(궤적)' 정보를 필요로 합니다. 이를 매 프레임 구해와야 합니다.

*   **변수 생성**:
    *   `CharacterTrajectory` (타입: `CharacterTrajectoryComponent` - Object Reference)

*   **Initialize Animation**:
    1.  `Get Owning Actor` -> `Has Component (CharacterTrajectoryComponent)` 확인.
    2.  있으면 `Get Component` 해서 `CharacterTrajectory` 변수에 저장.

*   **Update Animation**:
    *   (최신 엔진 버전에서는 `CharacterTrajectoryComponent`가 자동으로 궤적을 계산하므로, 별도 업데이트 로직이 필요 없을 수 있습니다. 단, 디버깅을 위해 변수가 유효한지 체크만 합니다.)

### 2-3. [Anim Graph] Motion Matching 노드 구성
실제 모션이 선택되는 곳입니다.

1.  **Motion Matching 노드 추가**: 그래프 우클릭 -> `Motion Matching` 검색.
2.  **설정 (Details Panel)**:
    *   **Searchable**: 아까 만든 **`CHT_Locomotion` (Chooser Table)**을 할당합니다. (**중요**)
    *   **Dynamic Trajectory**:
        *   이 핀을 노출시키거나(Pin 옵션), 기본적으로 내부에서 Owner의 `CharacterTrajectoryComponent`를 자동으로 찾습니다.
        *   명시적으로 연결하려면: `Property Access` 등을 통해 컴포넌트의 Trajectory 데이터를 연결합니다.
3.  **Settings**:
    *   `Blend Time`: 0.4초 (부드러운 전환을 위해 조정)
    *   `Max Distance`: 검색 범위 등 튜닝.
4.  **Output Pose**에 연결.

---

## 3. 캐릭터 연결

1.  사용 중인 캐릭터 블루프린트(`BP_MyCharacter`)를 엽니다.
2.  `Mesh` 컴포넌트 선택.
3.  `Anim Class`를 방금 만든 `ABP_Review_Mover`로 변경.
4.  **컴파일 & 플레이**.

## 4. 요약
1.  **데이터**: `PSD`에 애니메이션 넣고 -> `CHT(Chooser)`가 이를 선택하게 함.
2.  **블루프린트**: `Motion Matching` 노드 하나만 있으면 됨. 이 노드의 `Searchable`에 `CHT`를 넣음.
3.  **필수**: 캐릭터에 `Trajectory Component`가 달려 있어야 엔진이 미래 위치를 예측하고 매칭을 수행함.

---

## 5. 디버깅 팁 (Rewind Debugger)

적용 후 제대로 작동하는지 확인하려면 **Rewind Debugger**가 가장 강력한 도구입니다.
1.  `Tools` → `Debug` → `Rewind Debugger`를 엽니다.
2.  게임을 플레이(Simulate 또는 PIE)합니다.
3.  Rewind Debugger에서 해당 캐릭터(Actor)를 선택하고 녹화합니다.
4.  타임라인을 돌려보며 **Motion Matching** 트랙을 확인합니다.
    *   어떤 애니메이션이 선택되었는지(`Chosen Animation`)
    *   현재 포즈와 DB 내 포즈가 얼마나 일치하는지(`Cost`가 낮을수록 좋음)를 시각적으로 확인할 수 있습니다.

> 특히 **Trajectory** 설정과 **Schema(PSS)** 채널 튜닝이 까다로울 수 있으니, 막히면 이 두 부분을 우선 점검하세요.
