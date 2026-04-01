# 버그 리포트: Z축 배치 오류 (파묻힘, 장애물 위 배치 실패, 점프 아치 미작동)

**날짜:** 2026-03-23
**모듈:** ExRunnerPlayRuntime
**관련 시스템:** ItemManager (Z축 계산 로직)
**키워드:** `CalculateItemZ`, `GlobalPathDistance`, `AlphaInGap`, `ItemBaseZOffset`

## 증상 (Issue)
1. 생성된 코인이 바닥을 뚫고 절반 가량 파묻혀 스폰됨. (콜리전 반경에 따른 오프셋 부재)
2. Climb(올라기기) 장애물이 있는 구간에서도 코인이 장애물 위가 아닌 기본 바닥 높이에 스폰됨.
3. Gap(빈 공간) 구간에서 코인이 파라볼라(포물선)를 그리지 않고 일직선으로 배치되어 획득 불가.

## 원인 분석 (Root Cause)
1. 코인의 컬리전 캡슐 높이(Half Height)에 대한 Z축 기본 오프셋 프로퍼티가 매니저에 없어서 중심점이 지표면에 형성됨.
2. `QueryObstacleAtDistance` 함수 호출 시, 넘겨지는 인자 형식이 로컬 거리(`CurrentDistance`) 혹은 중앙값인 `PathDistance + CurrentDistance`로 잘못 переда됨. `Chunk->PathDistance`는 청크의 '중심'을 가리키기 때문에, 청크 길이에 절반(500)만큼 밀린 위치(앞쪽)로 오인하여 장애물을 찾지 못함.
3. Gap의 경우 `AlphaInGap`(점프 시작점부터 끝점까지의 0.0~1.0 비율)은 계산되었으나, 에디터 상에서 프로퍼티로 연동해둘 `JumpArcCurve` 커브 데이터가 할당되어 있지 않으면 기본값인 고정 높이로(`JumpApexHeight`) 평가되도록 작성됨.
4. `QueryObstacleAtDistance`가 장애물을 찾지 못한 경우엔 `bHasObstacle == false` 상태가 되어, 아예 새로 추가할 BaseOffset 연산(CalculateItemZ 호출)마저 건너뛰게끔 작성된 흐름 문제 존재.

## 해결 방법 (Resolution)
1. `ExRunnerItemManager` 헤더에 `ItemBaseZOffset` 변수(기본값 50.f)를 추가하고, `CalculateItemZ` 함수 내 모든 `switch-case` 반환 및 기본 반환문에 이 오프셋을 더하도록 일괄 변경.
2. 스폰 로직 상에서 글로벌 거리를 명시적으로 정확히 재계산하여 전달: `float GlobalPathDistance = Chunk->PathDistance - (Chunk->ChunkLength * 0.5f) + CurrentDistance;`
3. 커브(`JumpArcCurve`) 에셋이 할당되지 않은 경우, 엔진 내장 수학 함수(`FMath::Sin(AlphaInGap * UE_PI) * JumpApexHeight`)를 사용하여 아름다운 기본 점프 사인 포물선(Sine Curve) 궤적을 그리도록 로직 대응.
4. 장애물이 없을 때(`ObstacleManager` 질의 실패 시)에도 무조건 `CalculateItemZ`를 통과하도록 구조를 리팩토링하여 항상 기본 오프셋 패스가 적용되도록 개선.

## 결과 및 후속 조치
- 글로벌 좌표 매핑 정상화로 코인이 Climb 장애물을 정확히 감지해 위로 올라가게 되었습니다.
- Gap 구간에서 부드러운 사인 파형을 그리며 점프해 먹을 수 있도록 스폰됩니다.
- 에디터 내 ItemManager 디테일 패널에서 새로 추가된 `ItemBaseZOffset` 수치를 코인의 실제 반경(예: 40~60)에 맞춰 튜닝하실 수 있습니다.
