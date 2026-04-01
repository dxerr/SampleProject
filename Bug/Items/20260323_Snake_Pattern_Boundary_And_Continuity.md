# [2026-03-23] 코인 연속 스폰(뱀 패턴) 이탈 버그 및 중구난방 스폰 로직 개선

## 현상 (증상)
- 곡선 구간 등에서 코인이 `ChunkFloor` 바깥 허공으로 스폰되어 떨어지는 현상 발생.
- 코인 스폰 좌우 이동(Snake Pattern)이 청크마다 랜덤으로 초기화되거나 잦은 방향 전환 확률로 인해 매끄럽지 못하고 중구난방(Chaotic)으로 스폰됨.

## 원인 분석
1. **바깥 이탈 원인**: `SpawnTable`에 설정된 `MaxLateralOffset` 값이 현재 배치된 청크의 실제 폭(World Width)보다 클 경우, 코인이 청크 밖으로 나갈 수 있게 됨.
2. **중구난방 스폰 원인**: 
   - 매 청크마다 코인 라인이 스폰될 때 `CurrentLateralOffset`을 지역 변수(Local Variable)로써 랜덤으로 초기화함.
   - 중간 라인 이동 방향 변경 확률(`LateralDirectionChangeProbability`)로 인해 일관성 없이 지그재그가 무작위로 꺾임.

## 해결 방법 (수정 내용)
1. **청크 폭 기반 동적 경계 계산**:
   - `ExRunnerItemManager`의 `SpawnCoinLine` 시, 현재 청크의 `FloorMesh` 바운드 폭(`GetFloorBounds().GetExtent().Y * Scale`)을 구합니다.
   - 플레이어 충돌 범위와 코인 크기를 고려한 여유폭(`SafeMargin=70.0`)을 준 한계폭(`RealMaxOffset`)을 계산합니다.
   - 에디터 데이터 에셋의 설정값(`MaxLateralOffset`)과 비교하여 더 좁은 값을 실제 최대 한계(`EffectiveMaxOffset`)로 적용시켜 절대 바깥으로 튀어나가지 않도록 조치했습니다.
2. **연속적이고 일관된 상태 유지 (Stateful Drift)**:
   - `UExRunnerItemManager` 클래스 멤버 변수(`PersistentSnakeOffset`, `PersistentSnakeDir`)로 뱀 패턴의 마지막 위치와 진행 방향을 저장하도록 변경했습니다.
   - 청크가 바뀌어도 이전 스폰 위치와 진행 방향(부호)이 그대로 유지되어 자연스럽게 끊김 없이 이어집니다.
   - `LateralDirectionChangeProbability` 확률 기반 방향 반전 코드를 삭제하고, 경계선(1 혹은 -1 정규화 위치)에 닿았을 때만 무조건 방향을 반전하여 오직 안쪽으로만 전진하게 만들었습니다 (깔끔한 지그재그).
