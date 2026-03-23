# 버그 리포트: 코인(아이템) 스폰 시 회전축 왜곡 (Gimbal Lock) 오류

**날짜:** 2026-03-23
**모듈:** ExRunnerPlayRuntime
**관련 시스템:** RunnerItemManager (`SpawnCoinLine`, `SpawnBuffItem`)
**키워드:** `FRotator`, `FTransform`, Gimbal Lock, 곡선 청크 적용 오류

## 증상 (Issue)
스크린샷 및 테스트 결과, 곡선 및 경사로를 포함한 `ChunkFloor`에서 생성된 코인들이 바닥의 기울기 및 회전(Pitch, Roll, Yaw)과 완벽히 동기화되지 않고, 비정상적인 각도로 하늘을 찌르거나 삐뚤어지게 스폰되는 기현상이 발견됨. 

## 원인 분석 (Root Cause)
1. `ExRunnerItemManager`가 곡선 청크의 특정 거리 표면에서 로컬 좌표(`LocalTransform`)를 얻어온 후 이를 월드 스페이스로 회전시킬 때 수학적 오류가 발생.
2. 기존 수식: `FRotator SpawnRotation = (Chunk->GetActorRotation() + LocalTransform.Rotator());`
3. 3차원 공간에서 회전각 속성인 Pitch, Yaw, Roll은 독립적인 스칼라 값이 아니므로, **`FRotator` + `FRotator` 처럼 단순 덧셈으로 결합할 경우 "짐벌 락(Gimbal Lock)" 및 3차원 축 왜곡이 발생**함.
4. 청크가 이미 특정 월드 방향을 보고 있는 상태에서, 스플라인(Spline) 상의 로컬 회전값을 단순히 수치적으로 덧셈하면 벡터 공간상 틀어진 회전 행렬이 나옴.

## 해결 방법 (Resolution)
1. 언리얼 엔진의 정석적인 로컬-월드 변환 수학인 `FTransform` 행렬 곱셈을 사용하도록 구조 변경.
2. `FTransform GlobalTransform = LocalTransform * Chunk->GetActorTransform();` 처럼 로컬 좌표계 트랜스폼과 부모(청크) 트랜스폼을 행렬로 곱함.
3. 이후 계산된 완전한 월드 기준 `GlobalTransform` 에서 바로 `.Rotator()`와 `.GetLocation()`을 추출하여 스폰 파라미터로 사용함.

## 결과 및 후속 조치
- 로컬 `FRotator` 덧셈이 유발하던 회전각 왜곡이 원천 차단되었으며, 이제 곡선이나 가파른 기울기에서도 코인의 위쪽(Up 벡터)이 바닥의 수직 방향법선과 완벽하게 일치하게 됩니다.
