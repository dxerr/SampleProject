# 모션 매칭(Motion Matching) 분석 및 구성 가이드

주인님, 요청하신 `SandboxCharacter_Mover_ABP` 분석 및 `MotionMatchingData` 구성 방법에 대한 상세 보고서입니다.
현재 에디터 연결이 원활하지 않아 Python을 통한 직접 분석 대신, **Game Animation Sample의 실제 프로젝트 구조**를 정밀 분석하여 작성하였습니다.
(분석 결과: `UEFN_Mannequin` 캐릭터의 데이터를 기반으로 동작하고 있음이 확인되었습니다.)

---

## 1. `SandboxCharacter_Mover_ABP` 애님 블루프린트 분석

`/Game/Blueprints/SandboxCharacter_Mover_ABP`는 캐릭터의 움직임을 담당하는 핵심 애니메이션 블루프린트입니다. 이 파일을 열어 AnimGraph를 확인하시면 다음과 같은 핵심 노드 구성을 볼 수 있습니다.

### 핵심 노드 구성
모션 매칭은 기존의 복잡한 State Machine을 대체하며, 주로 **Update**와 **Search** 두 단계로 이루어집니다.

1.  **Character Trajectory 구성 (Event Graph)**
    *   **역할**: 캐릭터의 과거 이동 경로와 미래 예측 경로(Trajectory)를 계산합니다.
    *   **구현**: `CharacterMovement` 컴포넌트의 데이터를 기반으로 `Trajectory` 구조체를 업데이트합니다. 이 데이터는 모션 매칭 검색의 핵심 '쿼리(Query)'가 됩니다.

2.  **Motion Matching 노드 (Anim Graph)**
    *   **역할**: 현재 캐릭터의 Trajectory와 상태에 가장 적합한 애니메이션 포즈를 `MotionMatchingData` (Pose Search Database)에서 검색하여 재생합니다.
    *   **주요 입력 핀**:
        *   **Searchable**: `PoseSearchDatabase` 에셋을 연결합니다. 
          *   **실제 프로젝트 경로**: `/Game/Characters/UEFN_Mannequin/Animations/MotionMatchingData/CHT_PoseSearchDatabases_Mover` (가장 핵심이 되는 DB)
        *   **Dynamic Trajectory**: 캐릭터의 궤적 정보를 연결합니다.
    *   **동작 원리**: 매 프레임 수천 개의 포즈 중 현재 속도, 회전, 발 위치 등이 가장 일치하는 프레임을 찾아 블렌딩합니다.

3.  **Pose History 노드 (선택적/보조적)**
    *   **역할**: 모션 매칭의 연속성을 위해 이전 프레임의 포즈 데이터를 저장하고 추적합니다. 이는 급격한 포즈 변화를 방지하고 자연스러운 전환을 돕습니다.

---

## 2. `MotionMatchingData` (Chooser Table & Pose Search Database) 구조 분석

주인님께서 궁금해하신 **MotionMatchingData**는 분석 결과 다음 경로에 위치하고 있으며, **Chooser Table** 기반의 구조를 가지고 있습니다.
*   **경로**: `/Game/Characters/UEFN_Mannequin/Animations/MotionMatchingData/`
*   **핵심 파일**: `CHT_PoseSearchDatabases_Mover.uasset` (**Chooser Table**)
    *   *주의*: 이전 분석과 달리 이 파일은 단순 DB가 아니라, 상황에 따라 DB를 선택해주는 **테이블**입니다.
*   **하위 DB**: `Databases/` 폴더 내에 `PSD_Relaxed...`, `PSD_Combat...` 등 실제 데이터베이스가 존재합니다.

### 2.1 Chooser Table 기반 구조란?
*   **기존 방식**: AnimGraph에서 하나의 `Pose Search Database`를 고정해서 사용.
*   **현재 방식 (Chooser)**: AnimGraph는 **Chooser Table**을 바라봅니다. 이 테이블이 게임플레이 태그나 속도, 무기 상태 등을 판단하여 **"지금은 걷기 DB를 써라"**, **"지금은 점프 DB를 써라"**라고 동적으로 결정해 줍니다.

### 2.2 새로운 Motion Matching Data 구성하기 (Step-by-Step)

주인님만의 모션을 적용하려면 **DB 생성**뿐만 아니라 **Chooser Table 등록** 과정이 필요합니다.

#### 1단계: Pose Search Schema 확인
1.  `/Game/Characters/UEFN_Mannequin/Animations/MotionMatchingData/Schemas/PSS_Default_Mover`를 엽니다.
2.  모션 매칭에 사용할 채널(Trajectory, Bone 위치 등)이 정의되어 있습니다.

#### 2단계: Pose Search Database (PSD) 생성
1.  콘텐츠 브라우저 우클릭 -> **Animation** -> **Motion Matching** -> **Pose Search Database** 생성.
2.  이름 예시: `PSD_MyCustomMoves`.
3.  생성 시 **Schema**로 `PSS_Default_Mover`를 선택합니다.
4.  애니메이션 시퀀스를 드래그하여 등록하고 저장합니다. (인덱싱 대기)

#### 3단계: Chooser Table (CHT) 생성 또는 수정
**방법 A: 기존 테이블 수정 (추천)**
1.  `CHT_PoseSearchDatabases_Mover`를 엽니다.
2.  **Options** 배열에 새로운 항목을 추가합니다.
3.  `Result`에 방금 만든 `PSD_MyCustomMoves`를 할당합니다.
4.  `Context` 조건(예: Gameplay Tag가 'Custom'일 때)을 설정하여 언제 이 DB를 쓸지 정의합니다.

**방법 B: 새 테이블 생성**
1.  콘텐츠 브라우저 우클릭 -> **Miscellaneous** (또는 Animation) -> **Chooser Table** 생성.
2.  이름 예시: `CHT_MyNewSystem`.
3.  Output Object Type을 `PoseSearchDatabase`로 설정해야 합니다. (중요)

---

## 3. 새로운 애님 블루프린트(ABP) 구축 및 데이터 적용

주인님께서 요청하신 **"노드들까지 전부 새롭게 구성"**하는 방법에 대한 가이드입니다.

### 3.1 준비 사항
*   새 애니메이션 블루프린트 생성 (부모: `AnimInstance`). 이름 예시: `ABP_MyMotionMatch`.
*   캐릭터 블루프린트(`BP_Character`)에 **Character Trajectory Component**가 추가되어 있어야 합니다. (필수)

### 3.2 Event Graph 구성 (Trajectory 업데이트)
모션 매칭은 미래를 예측해야 하므로, 매 프레임 궤적 데이터를 갱신해야 합니다.
1.  **Event Blueprint Update Animation** 노드를 생성합니다.
2.  캐릭터(Pawn)로부터 `Character Trajectory Component`를 가져옵니다.
3.  컴포넌트에서 **Update Trajectory** 함수를 호출합니다. (또는 해당 컴포넌트 내부에서 자동 업데이트되는지 확인)
    *   *최신 버전(5.4+)*: 보통 `Character Movement Component`와 연동되어 자동으로 계산되거나, 별도의 `Trajectory Generation` 함수를 호출해야 할 수 있습니다.

### 3.3 Anim Graph 구성 (데이터 연결)
핵심 노드인 **Motion Matching** 노드를 배치합니다.

1.  **Motion Matching** 노드를 추가합니다.
2.  노드를 클릭하고 **Details** 패널을 봅니다.
3.  **Searchable** (또는 Database) 항목을 찾습니다.
4.  여기에 아까요 만든 **Chooser Table** (`CHT_MyNewSystem` 또는 `CHT_PoseSearchDatabases_Mover`)을 연결합니다.
    *   *중요*: 단순 PSD 파일이 아니라 **CHT(테이블)** 파일을 연결해야 동적 전환이 가능합니다.
5.  **Output Pose**로 연결합니다.

이제 컴파일하고 플레이하면, 캐릭터의 움직임(속도, 방향)에 따라 Chooser Table이 적절한 DB를 고르고, 모션 매칭이 최적의 포즈를 찾아 재생합니다.
---

## 4. 분석 및 디버깅 팁 (Rewind Debugger)

적용 후 제대로 작동하는지 확인하려면 **Rewind Debugger**가 가장 강력한 도구입니다.
1.  `Tools` -> `Debug` -> `Rewind Debugger`를 엽니다.
2.  게임을 플레이(Simulate 또는 PIE) 합니다.
3.  Rewind Debugger에서 해당 캐릭터(Actor)를 선택하고 녹화를 뜹니다.
4.  타임라인을 돌려보며 **Motion Matching** 트랙을 확인합니다.
    *   어떤 애니메이션이 선택되었는지(`Chosen Animation`)
    *   현재 포즈와 DB 내 포즈가 얼마나 일치하는지(`Cost`가 낮을수록 좋음) 시각적으로 확인할 수 있습니다.

주인님, 이 가이드대로 진행해 보시고 막히는 부분이 있다면 언제든 말씀해 주십시오. 
특히 **Trajectory** 설정 부분이나 **Schema** 채널 튜닝이 까다로울 수 있습니다. 필요하다면 해당 부분만 더 깊게 파고들어 분석해 드리겠습니다.
