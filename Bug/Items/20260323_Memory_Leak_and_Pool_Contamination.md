# 버그 리포트: 디자인 설정 유실 방지를 위한 풀링 끄기 및 메모리 누수 방지

**날짜:** 2026-03-23
**모듈:** ExCoreRuntime, ExRunnerPlayRuntime
**관련 시스템:** ItemSpawnManager, ObstacleManager (`OnChunkDespawned`)
**키워드:** `bUsePooling`, 풀 오염(Pool Contamination), `Destroy`, `BindToSpawner`

## 증상 (Issue)
1. 코인이 스폰될 때 블루프린트에서 세팅한 기본 회전값(Rotation) 등 디자이너 설정이 (0,0,0)으로 초기화되거나 덮어씌워짐.
2. 장시간 플레이 시 코인이 맵에 영구적으로 쌓이는 메모리 누수 발생 가능성 발견.
3. 일부 코인이 사라진 뒤 장애물 생성 턴에 갑자기 코인이 대신 스폰되는 비정상 동작 유발 가능성 발견.

## 원인 분석 (Root Cause)
1. `ExItemSpawnManagerBase`는 기본적으로 오브젝트 풀링을 사용해 재사용 시 `SetActorTransform`을 수행하기 때문에, Blueprint 에디터에서 잡아둔 컴포넌트들의 위치나 회전이 풀링 복원 과정에서 오작동하거나 덮어씌워질 여지가 컸음.
2. 청크가 소멸할 때 파생되는(`OnChunkDespawned`) 콜백이 `ExObstacleManager`에게만 연결되어 있고, `ExRunnerItemManager`에는 연결되어 있지 않아 코인 액터가 자동으로 회수되거나 파괴되지 않고 누수됨.
3. 가장 치명적인 점은, `ExObstacleManager::OnChunkDespawned` 내부에서 청크에 붙어있는 액터들(`GetAttachedActors`)을 정리할 때, 그게 장애물(Obstacle)인지 코인(Item)인지 **타입을 구분하지 않고 모조리 자신의 장애물 풀(`ObstaclePool`)에 집어넣어 버렸음**! 이 때문에 장애물을 스폰해야 할 타이밍에 풀에서 튀어나온 코인이 스폰될 수 있는 풀 오염(Pool Contamination) 상태였음.

## 해결 방법 (Resolution)
1. **풀링 옵션 부여 및 기본값 비활성화**
   - 디자이너의 초기 의도(Blueprint EventGraph 및 변수 설정)를 깔끔하게 보장하기 위해, `ExItemSpawnManagerBase`의 `bUsePooling` 프로퍼티 기본값을 `false`로 변경.
   - 풀링 미사용 시 `ReturnItemToPool` 호출 시 액터를 즉시 파괴(`Item->Destroy()`)하도록 로직 추가.
2. **풀 오염(Pool Contamination) 수정**
   - `ExObstacleManager::OnChunkDespawned` 내부에서 Attached 액터들을 회수할 때, 자신이 등록한 `ObstacleDefinitions` 배열의 클래스와 일치하는지(`IsA`) 타입 체크를 거치는 필터 파이프라인 추가.
3. **ItemManager 청크 연동(메모리 누수 방지)**
   - `ExRunnerItemManager` 헤더 및 cpp에 `BindToSpawner()` 및 `OnChunkDespawned()` 함수를 정의하고, 청크 소멸 시 해당 청크에 붙어있던 코인들만 정확히 캐치하여 파괴(`ReturnItemToPool(Item)`)하도록 작성.
   - `ExRunnerGameMode.cpp`에서 `ItemManager->BindToSpawner` 호출 구문을 누락 없이 추가.

## 결과 및 후속 조치
- 아이템 스폰 시 디자이너가 Blueprint 내부에서 조작하는 회전이나 기타 커스텀 값들이 그대로 유지됩니다!
- 더 이상 청크가 파괴될 때 코인이 화면 밖에 무한정 쌓이거나, 장애물 풀에 섞여들어가는 끔찍한 사태가 발생하지 않게 되었습니다. 코드는 완전히 안정화되었습니다.
