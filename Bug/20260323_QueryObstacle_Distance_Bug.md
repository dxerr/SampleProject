# 버그 리포트: 장애물 쿼리 시스템의 거리 계산 오류

**날짜:** 2026-03-23
**모듈:** ExRunnerPlayRuntime
**관련 시스템:** ObstacleManager (`QueryObstacleAtDistance`)
**키워드:** `PathDistance`, `GetActorLocation`, `DistToObstacle`

## 증상 (Issue)
이전 Z축 버그 업데이트(오프셋 추가)를 통해 코인이 땅에 파묻히는 현상은 해결되었으나, 여전히 여전히 Climb, Gap, Slide 같은 장애물이 코인 스폰 로직에 감지되지 않아 특수 Z배치(점프, 올라가기 등)가 적용되지 않고 모두 평지로 스폰되는 버그가 발생.

## 원인 분석 (Root Cause)
1. `ExRunnerItemManager`가 코인을 스폰할 때, 코인의 스폰 위치(글로벌 PathDistance)를 넘겨주어 주변에 장애물이 있는지 확인하는 `UExObstacleManager::QueryObstacleAtDistance` 함수를 호출함.
2. `QueryObstacleAtDistance` 내부에서 인자로 받은 `PathDist` (논리적 게임 진행 거리, 예: 2000.f)와 실제 스폰된 장애물 액터의 좌표를 비교하는데, 이 때 비교 대상이 `Attached->GetActorLocation().X` (월드 절대 좌표, 예: 15300.f)로 잘못 지정되어 있었음.
3. 캐릭터와 청크가 이동하며 월드 X값이 커질수록 `PathDist`와 `World X`값의 차이는 넘을 수 없을 정도로 벌어짐.
4. 결과적으로 `float DistToObstacle = FMath::Abs(PathDist - Attached->GetActorLocation().X);` 계산값이 언제나 `QueryRadius` 반경을 훌쩍 넘어가버려(`> 10000.f`), 코인의 위치가 장애물 내부임에도 장애물이 없다고 단정짓고 로직을 스킵(`continue`) 해버림.

## 해결 방법 (Resolution)
1. 이미 바로 위 줄에서 청크의 `PathDistance` 기반으로 장애물의 상대 위치를 잘 계산해 둔 `ObstacleLocalDist` 변수가 존재.
2. `DistToObstacle`을 계산할 때 월드 X 좌표가 아닌, `PathDist`의 동일 기준계인 `ObstacleLocalDist`를 빼도록 수정 조치.
   - `float DistToObstacle = FMath::Abs(PathDist - ObstacleLocalDist);`

## 결과 및 후속 조치
- 아이템 매니저가 드디어 월드 상의 장애물을 제대로 인식하게 됨.
- 인식된 장애물의 타입(Climb, Slide, Gap 등)에 맞춰 Z축 매니저 함수(`CalculateItemZ`)가 정상적으로 동작할 것입니다.
