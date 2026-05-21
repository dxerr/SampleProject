# 버그 해결 보고서: 코인 및 버프 아이템 스폰 위치 오프셋 누락 오류

## 1. 이슈 키워드
- `아이템 스폰 위치 고정`, `Lane Index 0 스폰`, `GenerateCoinLinePlan`, `CurrentLaneYOffset`

## 2. 발생 증상 및 문제 분석
- **증상**: 러너 게임 플레이 시 Coin 및 BuffItem이 지그재그나 뱀 패턴을 그리지 않고 무조건 트레드밀 바닥의 정중앙 라인(Lane Index 0)에서만 일렬로 스폰되는 현상 발생.
- **분석**:
  - 기존 구버전 스폰 시스템(`SpawnCoinLine`)에서 멀티플레이 정합성을 높이기 위해 도입된 계획-실체화 시스템(`GenerateCoinLinePlan`)으로 동기화 흐름이 전환됨.
  - 이 과정에서 새 함수 `GenerateCoinLinePlan` 루프 내부에 레인 오프셋(`CurrentLaneYOffset`)의 보간 연산 및 레인 너비(`LaneWidth`) 산출 수식이 이식되지 않은 채로 누락된 것이 원인으로 확인됨.

## 3. 트러블슈팅 및 해결 과정
- **레거시 제거**: 외부 호출이 전혀 없어 데드 코드가 된 기존 함수 `SpawnItemsOnChunk`, `SpawnCoinLine`, `SpawnBuffItem` 및 `GetCachedCoinRadius`를 헤더와 소스 파일에서 완전히 삭감하여 가독성을 증진함.
- **로직 수정 및 이식**:
  1. `GenerateCoinLinePlan` 함수의 시작 시점에 청크 Y Bounds를 3등분하여 `LaneWidth`를 구하는 공식을 추가함.
  2. 스폰 루프 내부에서 `CachedSpawnTable->bUseSnakePattern` 설정에 따라 `CurrentLaneYOffset`를 드리프트 보간 갱신하거나 혹은 즉각 변경 레인으로 대입하도록 수식을 복원함.
  3. 코인 라인이 도중에 끊기는 판정이 났을 시, 스플라인 흐름상 자연스러움을 위해 즉시 다음 목표 레인 좌표로 `CurrentLaneYOffset`가 스냅(Snap)되도록 보정함.
- **멀티플레이 정합성 검토**:
  - `GenerateCoinLinePlan`은 서버와 클라이언트 양측에서 공통적인 결정론적 시드 난수(`ItemRandomStream`)와 청크 스플라인의 법선 방향(`SplineRight`)을 바탕으로 순수 수학적 Plan 좌표를 도출하므로, Listen/Dedicated 환경에서 클라이언트가 복제(Replicate)된 액터를 렌더링할 때 완벽한 동기화 성능을 보장함을 확인함.
