# Bug Report: Climb WarpTarget Scale Offset 및 이중 스케일(Double Scale) 문제

## 이슈 개요
- **증상 1 (WarpTarget Offset)**: 장애물을 타겟팅하는 동작(Climb 등) 시 초록색 WarpTarget 포인트가 장애물의 중앙이 아닌, 우측 등 스케일에 비례하여 왜곡된 위치에 간헐적으로 찍히는 현상 발생. 
- **증상 2 (더블 스케일 및 Z축 비정상 상승)**: 장애물이 스폰될 때 Z축 등 높이 정보가 비정상적으로 높게 설정되어 화면 허공에 WarpTarget이 위치함(Double Scale).
- **증상 3 (풀링 스케일 충돌 / 눈덩이 현상)**: 재사용된 장애물(풀링 오브젝트)에서 스케일 변경 비율이 무시되어 작은 상자와 의도된 크기 상자가 번갈아가며 스폰되거나, 스플라인 포인트 좌표가 누적(눈덩이)으로 확장됨.

## 원인 분석 (Root Cause)

1. **언리얼 5 `FindLocationClosestToWorldLocation` 노드 버그 (비균일 스케일 이슈)**
    - 부모 액터(장애물)에 1x4x1 등 **비균일 스케일(Non-Uniform Scale)**이 적용된 경우, 
    - 언리얼 엔진의 스플라인 기반 수식 거리 계산 과정에서 내부적인 변환 오류(탄젠트 및 로컬 변환 오차)가 일어나 엉뚱한 위치를 반환함. 
    - 그 결과 캐릭터가 전방 중심부가 아닌 엉뚱한 스케일 편향점(우측 모서리 방향) 등을 타겟으로 삼게 됌.
    
2. **부모 상속에 의한 스플라인 `Double Scale` 현상**
    - 장애물의 외곽 영역을 정의하는 4개의 `SplineComponent`(`Ledge_1~4`)가 `StaticMeshComponent`의 **자식(Child)**으로 어태치(Attach)되어 있는 계층 구조. 
    - 부모 메쉬 컴포넌트 스케일을 증가시킬 때 자식인 스플라인도 부모의 스케일을 1차 상속받고,
    - 수동으로 스플라인 포인트 좌표(`LocationAtSplinePoint`)에 추가로 스케일을 곱하게 되면 스케일이 두 번 곱해지는 결과(TargetHeight * TargetHeight 등)를 낳아 Z축상으로 치솟게 됌.

3. **오브젝트 풀링 시 AABB 재조회 및 눈덩이 오류 (Pooling & Bounds Mismatch)**
    - 기존 Bounds 파악 로직인 `Mesh->Bounds`는 월드 스케일이 적용된 AABB를 반환. 장애물 재사용 시 남아있던 이전 스케일을 기반으로 BaseSize가 크게 잡혀 비율 계산이 1.0(비교적 작아짐)으로 튀는 지그재그 버그 발생.
    - 또한 스플라인 좌표에 곱셈을 반복하면서, 예전에 조작된 스플라인 포인트의 누적값에 다시 스케일업 비율을 곱해버리는 '눈덩이' 현상이 동반됨.

## 해결방안 (Resolution)

> **"액터(Actor) 스케일은 1.0으로 고정하고 내부 컴포넌트를 유연하기 역산, 보정하는 우회 전략 채택"**

### 상세 구현 로직 (`UExObstacleStrategy_Climb::ConfigureObstacle_Implementation` 및 `GetVisualBoundsOf_Climb`)

1. **액터(Actor) 전체 스케일 고정 (버그 1 차단)**
    - `Obstacle->SetActorScale3D(FVector::OneVector)`
    - 액터 스케일을 `1.0`으로 두어 `AC_TraversalLogic` 블루프린트의 거리 측정 계산 시 엔진 5 노드 오류(Non-Uniform Spline Calculation)를 우회.
    
2. **`StaticMeshComponent` 스케일 직접 지정 (버그 3-오브젝트 풀링 잔류 방지)**
    - `Mesh->GetStaticMesh()->GetBounds()`
    - 이전 AABB 잔류 오류를 막기 위해, 에셋의 원본(로컬) 바운딩 박스를 참조하도록 개선하여, 이전 사용 당시의 변형이나 회전에 의한 팽창값 오차 원천 배제.

3. **`SplineComponent` 부모 스케일 전파 역산 방어 (버그 2 차단)**
    -  `Spline->SetRelativeScale3D(FVector(1.0f / SafeScaleX, 1.0f / SafeScaleY, 1.0f / SafeScaleZ));`
    - 자식 컴포넌트인 스플라인이 부모(`StaticMesh`)의 스케일업을 강제 상속받는 것을 상쇄하기 위해, 자체적인 타겟 비율에 대해 **역수(1/Scale)를 로컬 스케일로 적용**하여 본연의 1.0 월드 스케일을 유지함.

4. **스플라인 눈덩이 확산 포인트 보정 (버그 3 차단)**
    - `FVector ScaleMultiplier(ScaleX / PreviousScale.X ...)`
    - 이전 장애물 상태(PreviousScale)와 비교하여, **부족한 비율분(추가 배율)만큼만 스플라인 포인트 좌표(`LocationAtSplinePoint`)를 연산 이동**하게 수정. 무한히 커지거나 작아지는 지그재그 버그 회피.

## 결론 및 참고
- `SetActorScale3D`의 잘못된 사용이 언리얼 엔진 코어 노드(비균일 스플라인)들의 오동작 트리거가 될 수 있음을 확인.
- C++ 헬퍼 코드에서 모든 컴포넌트를 명시적으로 찾아 부모와 자식 간 스케일 종속성을 끊고(Inverse Scale), 각 스플라인 포인트의 물리 좌표계(Local Relative)를 수식으로 수동 조작하여 완전한 제어권을 획득 완료함.
