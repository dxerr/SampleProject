# [Obstacle] Spline Scaling & Pooling "Snowballing" 통합 리포트

## 1. 개요
오브젝트 풀링(Object Pooling) 시스템에서 장애물(특히 Climb/Slide 장애물)을 재사용할 때, 자식 컴포넌트인 스플라인의 스케일이 이전 값을 누적하여 비정상적으로 거대해지거나(Snowballing), Motion Warping 대상(WarpTarget)의 위치가 어긋나는 현상을 해결했습니다.

## 2. 주요 문제 및 원인 분석

### A. 스플라인 스케일 눈덩이 현상 (`Snowballing`)
*   **증상**: 풀에서 꺼내올 때마다 장애물의 자식 스플라인 스케일이 기하급수적으로 커짐.
*   **원인**: `RestoreFromPool` 시점에 부모 액터의 스케일이 `(1,1,1)`로 초기화되지 않은 상태에서 자식 스플라인에 추가 스케일 값이 곱해짐 (Local Scale vs World Scale 충돌).
*   **해결**: `InitializeObstacle` 최상단에서 액터의 스케일을 강제로 `(1,1,1)`로 초기화하는 `ResetObjectScale()` 호출 보장.

### B. WarpTarget 오프셋 및 이중 스케일 (`Double Scaling`)
*   **증상**: Climb 장애물에서 캐릭터가 허공을 짚거나 텔레포트함.
*   **원인**:
    *   장애물 액터 자체가 비균등 스케일(Non-uniform Scale)을 가질 때, 자식인 WarpTarget 위치가 부모 스케일의 영향을 이중으로 받음.
    *   `GetVisualBoundsOf` 유틸리티가 스케일이 적용된 콜리전의 중심과 실제 비주얼 메시의 원점 차이를 계산하지 못함.
*   **해결**:
    *   `UExObstacleStrategy_Climb`: `ConfigureObstacle` 시점에 부모 스케일의 역행렬(Inverse Scale)을 WarpTarget에 적용하여 월드 좌표 고정.
    *   `CalculateObstacleScale`: 콜리전 익스텐트(Box Extent)가 아닌 실제 스플라인 경로와 메시 경계를 기준으로 스케일을 다시 산출하도록 유틸리티 통합.

## 3. 핵심 수정 파일
*   `UExObstacleStrategy_Climb.cpp`
*   `ExObstacleBase.cpp` (ResetObjectScale 추가)
*   `ExObstaclePoolComponent.cpp`
*   `ExSpawnUtility.h` (Bounds 계산 로직 통합)

## 4. 최종 결과
이제 모든 장애물은 풀에서 복구될 때 깨끗한 스케일 상태(`1.0`)로 시작하며, 어떠한 맵 스케일 환경에서도 캐릭터의 파쿠르 WarpTarget 위치가 100% 일치합니다.
