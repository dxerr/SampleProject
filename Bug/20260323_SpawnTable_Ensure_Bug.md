# 버그 리포트: SpawnTable 미할당 시 어서트 발생 버그

**날짜:** 2026-03-23
**모듈:** ExRunnerPlayRuntime
**관련 시스템:** ExRunnerItemManager
**키워드:** `ensureAlwaysMsgf`, `SpawnTable`, `Assert`, `TargetChunk`

## 증상 (Issue)
에디터 플레이 도중, 청크가 파괴되고 새로 생성되는 사이클(`OnChunkReachedKillZ` -> `SpawnNextChunk` -> `SpawnItemsOnChunk`)에서 다음 에러 콜스택과 함께 언리얼 에디터가 일시 정지(Assert Break) 됨.
```
[ExRunnerItemManager] SpawnTable이 할당되지 않았습니다!
```

## 원인 분석 (Root Cause)
1. `ExRunnerItemManager::SpawnItemsOnChunk` 함수 내에서 `TargetChunk`와 `SpawnTable` 포인터 유효성을 검사할 때, 언리얼 엔진의 디버그용 강제 중지 매크로인 `ensureAlwaysMsgf`를 사용했습니다.
2. 만약 에디터 블루프린트(`BP_ExRunnerGameMode` 등)에서 `ItemManager`의 `Spawn Table` 프로퍼티를 비워두었거나 아직 할당하지 않고 테스트할 경우, 널(null) 포인터 검사에 걸려 에디터가 정지되어 작업 흐름을 크게 방해했습니다.
3. 기획상 데이터 테이블을 굳이 할당하지 않으면 "아이템을 배치하지 않는 스테이지"로 조용히 넘어가야 하는데, C++ 코드 단에서는 이를 무조건적인 기획 누락 혹은 프로그래머의 실수(Assert 대상)로 취급하고 있었습니다.

## 해결 방법 (Resolution)
1. 공격적인 `ensureAlwaysMsgf` 매크로를 제거했습니다.
2. 유효하지 않은 `TargetChunk`나 `SpawnTable`이 감지되었을 때는 단순히 `UE_LOG`를 통해 `Warning` 수준의 노란색 경고 로그만 콘솔에 띄워주고, `return` 시켜 빈 청크가 생성되도록 자연스럽게 예외 처리했습니다.

## 결과 및 후속 조치
- 이제 블루프린트에서 `SpawnTable`을 실수로 비워 놓았거나 아이템 스폰을 원치 않는 상황이 되어도, 엔진에서 브레이크가 걸리지 않고 안정적으로 테스트를 이어갈 수 있습니다.
- "조용히 넘어가야 하는 예외 상황"과 "치명적인 설계 결함"을 명확히 구분하여 불필요한 `ensure` 남발을 지양합니다.
