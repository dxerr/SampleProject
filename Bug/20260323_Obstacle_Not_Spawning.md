# 버그 리포트: 장애물이 스폰되지 않는 문제

**날짜:** 2026-03-23
**모듈:** ExRunnerPlayRuntime
**관련 시스템:** ChunkSpawner, ObstacleManager 중앙 제어 시스템
**키워드:** `SpawnObstaclesOnChunk`, `BoundSpawner`, `ExRunnerGameMode`

## 증상 (Issue)
에디터 플레이 시, 바닥 청크 위에 아이템(코인)은 정상적으로 스폰되지만, 연결된 장애물이나 버프 아이템이 전혀 스폰되지 않음. (빈 바닥만 나오는 현상)

## 원인 분석 (Root Cause)
1. 중앙 제어 아키텍처(v1.2)로 변경하면서 `ExChunkSpawner`가 `ObstacleManager`와 `ItemManager`를 주입받아(`SetManagers`) 순차적으로 직접 호출(`SpawnObstaclesOnChunk -> SpawnItemsOnChunk`)하도록 로직을 변경함.
2. 이 과정에서 이중 스폰 방지를 위해 `ExRunnerGameMode::StartRunnerGame` 내부에 있던 `ObstacleManager->BindToSpawner(ChunkSpawner)` 호출을 실수로 함께 제거했음.
3. 하지만 `SpawnObstaclesOnChunk` 함수의 초입부에는 `if (!BoundSpawner || !BoundSpawner->ObstacleConfig) return;` 방어 로직이 존재했으며, `BindToSpawner`가 불리지 않아 `BoundSpawner`가 `nullptr` 상태로 남아 모든 장애물 스폰 로직이 실행 없이 반환(return)되어 버렸음.
4. 버프 아이템의 경우, 버프 자체의 스폰 확률(`BuffSpawnProbability`, `BuffSoloSpawnProbability`)이 낮게 설정되어 있거나, 에디터 데이터 에셋(`DA_ExRunnerItemSpawnTable`)의 `BuffEntries` 배열에 버프 정보가 채워져 있지 않으면 나타나지 않음.

## 해결 방법 (Resolution)
1. `ExRunnerGameMode.cpp`의 `StartRunnerGame` 함수에서 `ObstacleManager->BindToSpawner(ChunkSpawner)` 호출 구문을 다시 복구함.
   - 단, `ExObstacleManager::BindToSpawner` 내부에서는 기존의 `OnChunkSpawned` 델리게이트 바인딩은 완전히 제거된 상태를 유지하여 중복 스폰을 방지하고, 오직 `BoundSpawner` 참조 캐싱과 `OnChunkDespawned`(장애물 오브젝트 풀 회수용) 이벤트만 등록하도록 정리 유지함.
2. 버프 아이템 미생성 문제는 확률 요소와 에디터 데이터 에셋(DA) 세팅의 문제일 가능성이 제일 높음. 

## 결과 및 후속 조치
장애물 스폰 시 필수 참조 누락으로 인한 강제 배제 현상이 수정됨. 다시 컴파일 후에는 코인이 잘 배치되는 원리로 동일하게 장애물도 생성될 것임. 추가로 에디터 내 데이터 애셋에 버프 엔트리가 정상적으로 채워져 있는지, 또 테스트를 위해 100% 확률로 등장하게 임시 설정 후 테스트할 것을 권장.
