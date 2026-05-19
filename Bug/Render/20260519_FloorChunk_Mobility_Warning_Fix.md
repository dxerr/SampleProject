# 트레드밀 스크롤 이동 시 바닥 컴포넌트 모빌리티 경고 조치 보고서

> **분류**: 렌더링 및 액터 모빌리티 (Render)  
> **엔진**: UE 5.7.3  
> **작성일**: 2026-05-19  
> **보고자**: Antigravity (AI Coding Assistant)  
> **보고 대상**: 주인님 (Master)

---

## 1. 현상 요약 (Problem)
무한 러너 게임 플레이 중, 로그 창 및 콘솔에 아래와 같은 액터 컴포넌트의 Mobility(모빌리티) 관련 경고(`Warning`)가 대량으로 실시간 발생하여 로그가 극심하게 도배되는 현상 발생:
```
Mobility of /ExRunnerPlay/Map/UEDPIE_0_L_ExRunnerTest.L_ExRunnerTest:PersistentLevel.BP_ExFloorChunk_C_11 : GapFloor_Left has to be 'Movable' if you'd like to move. 
Mobility of /ExRunnerPlay/Map/UEDPIE_0_L_ExRunnerTest.L_ExRunnerTest:PersistentLevel.BP_ExFloorChunk_C_11 : GapFloor_Right has to be 'Movable' if you'd like to move. 
```

---

## 2. 원인 분석 (Root Cause)

### ① 트레드밀 스크롤러 물리 이동 메커니즘
- 무한 러너 게임의 지형 스폰 및 스크롤은 `ChunkSpawner`와 각 `ExFloorChunk` 액터들이 런타임에 위치(Actor Location)를 플레이어 진행 방향 뒤쪽으로 지속적으로 좌표 이동시키는 **트레드밀(Treadmill) 방식**으로 동작합니다.
- 부모 액터(`AExFloorChunk`)의 위치가 매 프레임 이동하므로, 액터에 부착된 모든 자식 컴포넌트들도 함께 물리적인 이동이 가능하도록 설정되어야 합니다.

### ② 동적 생성 컴포넌트들의 Stationary(고정) 하드코딩
- `ExFloorChunk.cpp` 내부 코드에서 장애물(구멍) 및 곡선 구간을 구현하기 위해 런타임에 컴포넌트들을 동적으로 인스턴스화하고 있습니다:
  - 구멍(Gap) 직선 편평 지형: `GapFloor_Left`, `GapFloor_Right` (Static Mesh Component)
  - 곡선(Curve) 원호 지형: `CurveSpline` (Spline Mesh Component)
  - 구멍(Gap) 곡선 지형: `GapCurveSpline` (Spline Mesh Component)
- 그러나 이 동적 생성 함수들 내부에서 모빌리티가 다음과 같이 **`EComponentMobility::Stationary`**로 강제 하드코딩 설정되어 있었습니다:
  - 예: `LeftFloor->SetMobility(EComponentMobility::Stationary);`
  - 예: `SplineMesh->SetMobility(EComponentMobility::Stationary);`
- 부모 액터가 이동할 때 자식 컴포넌트들이 `Stationary`로 고정 선언되어 있었기 때문에, 엔진에서 좌표 모순을 발견하고 매 이동 시도마다 **"움직이고 싶다면 모빌리티를 Movable로 변경해야 한다"**는 경고를 대량으로 출력한 것입니다.

---

## 3. 해결 조치 사항 (Solution)

`ExFloorChunk.cpp` 파일 내부에서 런타임 이동을 수행하는 부모 액터와 이에 소속된 모든 동적 컴포넌트들의 모빌리티 설정을 **`Movable (이동 가능)`**로 일관되게 치환 및 동기화하였습니다.

### [수정 파일]
*   [ExFloorChunk.cpp](file:///c:/wz/ExFrameWork/Plugins/GameFeatures/ExRunnerPlay/Source/ExRunnerPlayRuntime/Actors/ExFloorChunk.cpp)

### [세부 수정 내역]
1.  **기본 생성자 보완 (SceneRoot 및 FloorMesh)**:
    - 부모 SceneRoot와 기본 FloorMesh의 모빌리티를 명시적으로 `Movable`로 세팅하여 에디터 기본값과 상관없이 안전한 이동을 보장합니다:
    ```cpp
    SceneRoot->SetMobility(EComponentMobility::Movable);
    FloorMesh->SetMobility(EComponentMobility::Movable);
    ```
2.  **구멍 직선 편평 지형 (`GapFloor_Left` / `GapFloor_Right`)**:
    - `ApplyGap` 함수 내 동적 컴포넌트 생성 부분의 모빌리티를 `Movable`로 수정:
    ```cpp
    LeftFloor->SetMobility(EComponentMobility::Movable);
    RightFloor->SetMobility(EComponentMobility::Movable);
    ```
3.  **곡선 원호 지형 (`CurveSpline`)**:
    - `ApplyCurve` 함수 내 동적 스플라인 메시 컴포넌트 생성부의 모빌리티를 `Movable`로 수정:
    ```cpp
    SplineMesh->SetMobility(EComponentMobility::Movable);
    ```
4.  **곡선 구멍 지형 (`GapCurveSpline`)**:
    - `SpawnGapSplineMesh` 헬퍼 함수 내 동적 스플라인 메시 컴포넌트 생성부의 모빌리티를 `Movable`로 수정:
    ```cpp
    SplineMesh->SetMobility(EComponentMobility::Movable);
    ```

---

## 4. 조치 결과 및 기대 효과
- **콘솔 Warning 로그 완벽 소멸**: 매 프레임 수십 줄씩 화면과 콘솔을 붉게 도배하던 모빌리티 불일치 경고가 완전히 사그라들며 디버깅 쾌적성이 극대화됩니다.
- **성공적인 렌더링 무결성**: 런타임에 청크의 위치가 이동하거나 구멍이 뚫릴 때, 렌더링 드로우 콜 및 드롭 섀도우가 깨지지 않고 부드럽고 완벽하게 동작합니다.
