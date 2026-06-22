# ExRunner 시스템 아키텍처 (ExRunner System Architecture)

**최종 업데이트**: 2026-06-22
**프로젝트**: ExCore / ExFrameWork

> **2026-06-22 개정 메모:** 초기 트레드밀 구현(절대 X좌표 기반 `ShiftWorld` 방식)은 **커브 월드(곡선 트랙) 지원**과 멀티플레이 대응을 위해 **경로 누적 거리(PathDistance) 기반 + UE Mover 시뮬레이션** 방식으로 전면 교체되었습니다. §2·§3.2·§4.2·§5·§7.1·§7.4를 실제 코드에 맞춰 갱신했습니다.

---

## 1. 개요 (Overview)
ExRunner는 **무한 러너(Infinite Runner)** 게임 플레이를 지원하기 위한 모듈형 시스템입니다. 바닥·장애물·아이템이 끊임없이 생성·재활용되는 **트레드밀(Treadmill)** 방식을 채택하되, 좌표 추적은 단순 X축이 아니라 **경로를 따라가는 1차원 누적 거리(PathDistance)** 로 관리하여 **곡선(Curved World) 트랙**까지 지원합니다.

---

## 2. 핵심 시스템: 경로 거리 기반 트레드밀 (PathDistance Treadmill)

### 2.1 개념
"트레드밀"은 **무한히 이어지는 트랙을 유한한 청크 풀로 표현**하는 기법입니다. 핵심은 모든 배치·수명 판정을 월드의 절대 X좌표가 아니라 **경로를 따라 누적되는 1차원 거리값(PathDistance)** 으로 수행하는 것입니다.

> **왜 거리 기반인가:** 트랙이 곡선이면 "전진" 방향이 지점마다 달라져 X축 한 방향 기준이 성립하지 않습니다. 경로 거리(스플라인 누적 길이)를 기준 좌표계로 삼으면, 직선/곡선 구간 구분 없이 동일한 로직으로 스폰·재활용·동기화를 처리할 수 있습니다.

> **(레거시 참고)** 초기 버전은 "플레이어를 X=0에 고정하고 매 프레임 모든 월드 액터를 -DeltaX로 시프트(`ShiftWorld`)"하는 절대 좌표 방식이었으나, 곡선 트랙을 표현할 수 없어 폐기되었습니다. 관련 시행착오는 §7.1·§7.4 참조.

### 2.2 구현 상세
*   **전진 처리 주체**: 각 캐릭터의 `UExRunnerMovementComponent`. 이 컴포넌트는 **UE Mover 플러그인의 `IMoverInputProducerInterface`** 를 구현하여 `ProduceInput`으로 전진 입력을 주입하고, 실제 이동은 Mover 시뮬레이션이 수행합니다. (옛 수동 `AddActorWorldOffset` 보정은 Mover의 CorrectionInput으로 통합됨)
*   **거리 회계**:
    1.  각 캐릭터 MovementComponent가 자신의 진행 거리를 계산해 `PlayerState`에 동기화.
    2.  `AExRunnerGameState`가 이를 취합하여 `CurrentPathDistance` / `LeadDistance` / `SegmentStartDistance` 등 매치 전역 거리값을 관리.
    3.  `UExPathManager`가 경로(직선/곡선 세그먼트)와 거리→트랜스폼 변환을 담당.
*   **멀티플레이 대응**: 이동 로직이 GameMode가 아닌 **각 캐릭터의 MovementComponent**로 이관되어, 플레이어별 독립 진행과 클라이언트 보간이 가능합니다. (`AExRunnerGameMode::Tick`은 더 이상 이동을 직접 처리하지 않음)

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
*   **KillZ 처리 (거리 기반)**:
    *   각 청크가 자신의 `PathDistance`와 플레이어의 `CurrentPathDistance`를 비교하여, 플레이어 뒤쪽 한계(`PathDistance < CurrentPathDistance + KillZ`, `KillZ`는 음수 오프셋)를 넘어가면 풀로 반환·재활용.
    *   판정은 `AExFloorChunk::Tick`에서 수행하며, `OnChunkReachedKillZ` 델리게이트로 스포너에 반환을 위임.

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

### 4.2 상호작용 (GameplayTag 기반 Traversal 이벤트)
*   **역할**: 장애물과의 충돌/통과 상호작용을 처리.
*   **방식 (현행)**: 직접 `SetInteractionTarget` 함수를 호출하던 옛 방식 대신, **GameplayTag 이벤트**(`OnTraversalStart` / `OnTraversalEnd`)로 파쿠르·볼팅 등의 시작·종료를 알립니다.
    *   `AExRunnerGameMode`가 `TAG_Ex_Action_Climb_Start` 등의 이벤트를 구독하여 `bIsTraversing` 플래그를 갱신하고, 트래버설 중에는 회전 오버라이드를 일시 정지하는 등 게임 흐름을 조정합니다.
    *   모션 워핑 타겟 정합은 Motion Warping 컴포넌트 + 트래버설 이벤트 흐름으로 처리됩니다. (참조: `Guides/ExRunnerPlay/Climb_Sync_Guide.md`)

> **(레거시 참고)** 옛 `ExRunnerMovementComponent::SetInteractionTarget` / `ClearInteractionTarget` 경로는 제거되었습니다. 관련 초기 이슈 기록은 `Archive/Issues/Issue_Obstacle_Sync_Report.md` 참조.

---

## 5. 캐릭터 무브먼트 (`UExRunnerMovementComponent`)

### 5.1 역할
`APawn`(Mover 사용 Pawn)에 부착되어 러너 게임 특화 이동을 담당합니다. 상위 Pawn의 `UMoverComponent`에 **InputProducer로 등록**되어 입력을 주입합니다.

*   **자동 달리기 (Auto-Run)**: 전방 의도(Directional Intent)를 매 시뮬레이션 스텝 `ProduceInput`으로 주입하여 자동 전진. 진행 거리(PathDistance)의 원동력.
*   **레인 시스템 (Lane System)**: 좌우 이동을 로컬 벡터 기준으로 처리하여 커브 길에서도 안정적. (`CurrentLaneIndex`는 복제됨)
*   **상호작용 처리**: 트래버설 GameplayTag 이벤트(§4.2)와 연동하여 점프/파쿠르 로직 처리.

---

## 6. 개발 가이드라인 준수
*   모든 클래스는 **Ex** 접두사를 사용 (`AExFloorChunk`, `UExChunkSpawner` 등).
*   데이터 기반 설계: `UExObstacleDefinition` 등을 통해 기획 데이터 분리.

---

## 7. 주요 시행착오 (Trial and Error) 및 해결

### 7.1 무한 스폰 끊김 (Infinite Spawning Gap)
*   **문제**: 트레드밀 방식에서 `NextSpawnX` 전역 변수를 계속 누적(`+= ChunkSpacing`)하여 사용할 경우, 월드 이동 시점과 맞물려 좌표 불일치 발생. 스폰이 끊기거나 겹치는 현상.
*   **해결**: **상대 기준(Relative Implementation)** 방식 도입.
    *   항상 마지막 활성 청크 기준으로 `+ Spacing` 위치에 다음 청크를 스폰하여 상대 간격을 보장.

> **현행(V2):** 스폰 기준 변수는 X좌표(`NextSpawnX`, 현재 *레거시*로 표기됨)에서 **경로 누적 거리(`NextSpawnDistance`)** 로 전환되었습니다. 청크 배치는 `UExChunkSpawner::SpawnNextChunk()`가 `UExPathManager`와 연동해 거리값 기준으로 수행합니다.

### 7.2 청크 투명 현상 (Visibility & Lifecycle)
*   **문제**: 오브젝트 풀에서 청크를 재사용(`GetChunkFromPool`)할 때, `SetActorHiddenInGame(false)` 만으로는 메시 가시성이나 물리 상태가 정상적으로 초기화되지 않는 현상(투명 청크). LIFO(Stack) 방식 사용 시 즉시 재사용됨에 따라 초기화 시간 부족 가능성.
*   **해결**:
    *   **API 표준화**: `ActivateChunk` / `DeactivateChunk` 함수를 명시적으로 호출하여 `BP` 레벨 및 컴포넌트 단위의 상태 리셋 보장.
    *   **FIFO 전환**: 풀링 정책을 **LIFO(후입선출) → FIFO(선입선출)**로 변경. 가장 먼저 반환되어 "충분히 쉰" 청크를 재사용함으로써 렌더 및 물리 상태 안정화.

### 7.3 청크 즉시 소실 (Double Deactivation)
*   **문제**: `Tick` 함수 내에서 `OnChunkReachedKillZ` 델리게이트를 송출한 뒤, 연이어 `ReturnToPool`을 호출함. 스포너가 델리게이트를 받아 즉시 재활용(위치 이동 및 활성화)을 마쳤는데, 직후 `Tick`의 남은 로직이 실행되어 **방금 활성화된 청크를 다시 끔**.
*   **해결**: `Tick` 내 중복 호출 로직(`ReturnToPool`) 제거. 생명주기 제어권을 스포너(Delegate)에게 전적으로 위임.

### 7.4 장애물 생성 중단 (Coordinate Drift) — *V1에서 발생, V2에서 구조적으로 해소*
*   **문제 (V1)**: `ShiftWorld`로 월드 좌표가 계속 0 근처로 재설정되지만, `ExObstacleManager`가 기억하는 `LastObstacleSafeEndX`(안전 생성 지점)는 이 이동을 반영하지 않음. 시간이 지나면 안전 지점 좌표가 수만 단위로 커져서 현재 청크 범위를 벗어난 것으로 판단, 생성을 영구 중단함.
*   **당시 해결책 (V1)**: `UExChunkSpawner`에 `OnWorldShifted` 델리게이트를 추가하고, `ExObstacleManager`가 이를 구독하여 이동량(`DeltaX`)만큼 `LastObstacleSafeEndX`도 함께 보정.

> **현행(V2) — 이슈 자체가 사라짐:** 좌표계가 절대 X에서 **경로 누적 거리**로 바뀌면서 "월드 시프트와 안전지점 좌표의 어긋남"이라는 문제 원인 자체가 사라졌습니다. 안전지점 변수는 `LastObstacleSafeEndX` → **`LastObstacleSafeEndDistance`** 로 교체되었고, 플레이어 진행과 함께 자연스럽게 증가하는 거리값이라 보정이 불필요합니다. 따라서 **`ShiftWorld` / `OnWorldShifted`는 코드에서 완전히 제거**되었습니다. (전환 이력: `ExRunner_CurvedFloor_System_Architecture.md` 변경이력 2026-02-12 "KillZ PathDistance 기반 전환")

### 7.5 투명 장애물 (Component Visibility Reset)
*   **문제**: 장애물 액터를 풀링할 때 `SetActorHiddenInGame(false)`를 호출해도, 내부에 포함된 `StaticMeshComponent` 등의 렌더링 상태가 즉시 갱신되지 않아 게임 내에서 투명하게 보이는 현상.
*   **해결**:
    *   **강제 초기화**: `ActivateObstacle` 함수 구현. 액터 내부의 모든 `UPrimitiveComponent`를 순회하며 `SetVisibility(true, true)`를 강제 호출.
    *   **FIFO 풀링**: 장애물 풀 역시 **FIFO(선입선출)**로 변경하여 재사용 대기 시간을 확보.
