# ExRunner 시스템 아키텍처 (ExRunner System Architecture)

**최종 업데이트**: 2026-02-04
**프로젝트**: ExCore / ExFrameWork

---

## 1. 개요 (Overview)
ExRunner는 **무한 러너(Infinite Runner)** 게임 플레이를 지원하기 위한 모듈형 시스템입니다. `World Shift` 기술을 사용하여 캐릭터는 제자리에 고정되고 세상이 움직이는 **트레드밀(Treadmill)** 방식을 채택했습니다.

---

## 2. 핵심 시스템: World Shift (트레드밀)

### 2.1 개념
쉐이더(Curved World) 정밀도와 좌표계 안정성을 위해, **플레이어의 X 좌표를 원점(0) 근처로 유지**합니다.
플레이어가 이동하려 하면, 그 이동량(`Delta`)만큼 **모든 월드 액터(바닥, 장애물)를 반대로 이동**시킵니다.

### 2.2 구현 상세
*   **주체**: `UExRunnerMovementComponent` (TickComponent)
*   **로직**:
    1.  프레임 당 이동 거리(`DeltaX`) 계산.
    2.  `UExChunkSpawner::ShiftWorld(-DeltaX)` 호출.
    3.  플레이어 위치를 다시 원점(Fixed Origin)으로 리셋.
*   **장점**:
    *   부동 소수점 오차 방지.
    *   WPO(World Position Offset) 쉐이더가 항상 원점 기준으로 정확하게 적용됨.

---

## 3. 청크 스폰 시스템 (`UExChunkSpawner`)

### 3.1 역할
무한한 바닥(`AExFloorChunk`)을 생성하고 관리하며, 트랙의 연속성을 보장합니다.

### 3.2 주요 기능
*   **FIFO 오브젝트 풀링 (Smart Pooling)**: 
    *   Queue 구조를 사용하여 방금 반환된 청크가 즉시 재사용되어 발생하는 렌더링/물리 이슈 방지.
    *   초기 버퍼(Reserve) 확보.
*   **자동 간격 감지 (Auto-Spacing)**:
    *   `GetActorBounds`를 사용하여 메시의 실제 크기를 런타임에 측정.
    *   스케일이 달라도 빈틈없이 이어지도록 배치.
*   **KillZ 처리**:
    *   월드가 뒤로 밀려나 `KillZ` 좌표(플레이어 뒤쪽)를 넘어가면 풀로 반환 및 재활용.

---

## 4. 장애물 시스템 (Obstacle System)

### 4.1 구조 (Component-Based)
별도의 래퍼 액터(`ExObstacleActor`) 없이, **일반 `AActor` 기반 블루프린트**를 직접 사용합니다.

*   **스폰 방식**: `UExObstacleDefinition` 데이터 에셋에 정의된 `ObstacleClass`를 직접 스폰.
*   **가변 스케일링 (Generic Scaling)**:
    *   스폰 직후 액터의 Bounds를 측정하여, 데이터 에셋에 정의된 목표 크기(Random Range)에 맞게 `Scale3D` 자동 조정.
*   **피벗 보정 (Center Pivot)**:
    *   모든 장애물과 바닥은 **중앙(Center)** 피벗을 기준으로 정렬됩니다.
    *   `Z-Stacking`: `FloorHeight + ObstacleHeight`를 계산하여 바닥 위에 정확히 안착.

### 4.2 상호작용 (`UExObstacleInteractionComponent`)
*   **역할**: 장애물 BP에 부착되어 플레이어와의 충돌(Overlap)을 감지.
*   **기능**:
    *   플레이어가 닿으면 `ExRunnerMovementComponent`에 **상호작용 타겟(Target Component)**을 전달.
    *   파쿠르, 볼팅(Vaulting) 등의 모션 워핑 타겟지점으로 사용됨.

---

## 5. 캐릭터 무브먼트 (`UExRunnerMovementComponent`)

### 5.1 역할
`ACharacter`나 `APawn`에 부착되어 러너 게임 특화 이동을 담당합니다.

*   **자동 달리기 (Auto-Run)**: 전방 벡터 기준 자동 이동 처리 (World Shift의 원동력).
*   **레인 시스템 (Lane System)**: 좌우 이동을 로컬 벡터 기준으로 처리하여 커브 길에서도 안정적.
*   **상호작용 처리**: 장애물 컴포넌트로부터 타겟 정보를 받아 점프/파쿠르 로직 연동.

---

## 6. 개발 가이드라인 준수
*   모든 클래스는 **Ex** 접두사를 사용 (`AExFloorChunk`, `UExChunkSpawner` 등).
*   데이터 기반 설계: `UExObstacleDefinition` 등을 통해 기획 데이터 분리.

---

## 7. 주요 시행착오 (Trial and Error) 및 해결

### 7.1 무한 스폰 끊김 (Infinite Spawning Gap)
*   **문제**: 트레드밀 방식(World Shift)에서 `NextSpawnX` 전역 변수를 계속 누적(`+= ChunkSpacing`)하여 사용할 경우, 월드 이동 시점과 맞물려 좌표 불일치 발생. 스폰이 끊기거나 겹치는 현상.
*   **해결**: **상대 좌표(Relative Implementation)** 방식 도입.
    *   항상 `ActiveChunks.Last()`(마지막 활성 청크)의 현재 위치를 기준으로 `+ Spacing`위치에 다음 청크를 스폰.
    *   월드가 이동해도 상대적 간격은 유지되므로 완벽한 연결 보장.

### 7.2 청크 투명 현상 (Visibility & Lifecycle)
*   **문제**: 오브젝트 풀에서 청크를 재사용(`GetChunkFromPool`)할 때, `SetActorHiddenInGame(false)` 만으로는 메시 가시성이나 물리 상태가 정상적으로 초기화되지 않는 현상(투명 청크). LIFO(Stack) 방식 사용 시 즉시 재사용됨에 따라 초기화 시간 부족 가능성.
*   **해결**:
    *   **API 표준화**: `ActivateChunk` / `DeactivateChunk` 함수를 명시적으로 호출하여 `BP` 레벨 및 컴포넌트 단위의 상태 리셋 보장.
    *   **FIFO 전환**: 풀링 정책을 **LIFO(후입선출) → FIFO(선입선출)**로 변경. 가장 먼저 반환되어 "충분히 쉰" 청크를 재사용함으로써 렌더 및 물리 상태 안정화.

### 7.3 청크 즉시 소실 (Double Deactivation)
*   **문제**: `Tick` 함수 내에서 `OnChunkReachedKillZ` 델리게이트를 송출한 뒤, 연이어 `ReturnToPool`을 호출함. 스포너가 델리게이트를 받아 즉시 재활용(위치 이동 및 활성화)을 마쳤는데, 직후 `Tick`의 남은 로직이 실행되어 **방금 활성화된 청크를 다시 끔**.
*   **해결**: `Tick` 내 중복 호출 로직(`ReturnToPool`) 제거. 생명주기 제어권을 스포너(Delegate)에게 전적으로 위임.

### 7.4 장애물 생성 중단 (Coordinate Drift)
*   **문제**: 트레드밀 시스템(`ShiftWorld`)으로 인해 월드 좌표가 계속 0 근처로 재설정(-DeltaX 이동)되지만, `ExObstacleManager`가 내부적으로 기억하는 `LastObstacleSafeEndX`(안전 생성 지점) 변수는 이 이동을 반영하지 않음. 시간이 지나면 안전 지점 좌표가 수만 단위로 커져서, 현재 청크 범위(0~1000)를 벗어난 것으로 판단, 생성을 영구 중단함.
*   **해결**:
    *   **이벤트 동기화**: `UExChunkSpawner`에 `OnWorldShifted` 델리게이트 추가.
    *   **좌표 보정**: `ExObstacleManager`가 이를 구독하여, 월드 이동량(`DeltaX`)만큼 `LastObstacleSafeEndX` 변수도 함께 이동(감소)시킴.

### 7.5 투명 장애물 (Component Visibility Reset)
*   **문제**: 장애물 액터를 풀링할 때 `SetActorHiddenInGame(false)`를 호출해도, 내부에 포함된 `StaticMeshComponent` 등의 렌더링 상태가 즉시 갱신되지 않아 게임 내에서 투명하게 보이는 현상.
*   **해결**:
    *   **강제 초기화**: `ActivateObstacle` 함수 구현. 액터 내부의 모든 `UPrimitiveComponent`를 순회하며 `SetVisibility(true, true)`를 강제 호출.
    *   **FIFO 풀링**: 장애물 풀 역시 **FIFO(선입선출)**로 변경하여 재사용 대기 시간을 확보.
