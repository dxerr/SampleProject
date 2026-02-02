# 무한 러너 청크 스포너 구현 상세 보고서 (Infinite Runner Chunk Spawner Implementation Report)

**작성일**: 2026-02-02
**작성자**: Antigravity (Assistant)
**프로젝트**: ExCore / ExFrameWork

---

## 1. 개요 (Overview)
본 문서는 `ExFrameWork` 기반의 러너 게임에서 무한히 생성되는 바닥(청크)을 효율적으로 관리하기 위해 구현된 `UExChunkSpawner` 컴포넌트와 `AExFloorChunk` 액터의 기술적 상세 내용, 문제 해결 과정, 그리고 최종 아키텍처를 기술합니다.

## 2. 핵심 기능 및 아키텍처 (Core Architecture)

### 2.1 스마트 오브젝트 풀링 (Smart Object Pooling with FIFO)
퍼포먼스 최적화와 메모리 파편화 방지를 위해 **오브젝트 풀링(Object Pooling)** 패턴을 적용했습니다.

*   **기존 문제 (LIFO 방식의 한계)**:
    *   초기에 `Stack(LIFO)` 구조를 사용했으나, '방금 반환된(Deactivated)' 청크가 '곧바로 다시 재사용(Activated)'되는 현상이 발생했습니다.
    *   이로 인해 **동일 프레임(Same Frame)** 내에서 `Deactivate`와 `Activate`가 연쇄적으로 호출되며 렌더링 상태 갱신 누락, 가시성 꼬임(Flickering), 로직 충돌 등의 문제가 발생했습니다 ("태어나자마자 죽는 현상").

*   **해결책 (FIFO + Reserve Buffer)**:
    *   **Queue(FIFO) 구조 도입**: 풀링 자료구조를 스택에서 큐로 변경하여, 가장 오래전에 반환되어 '충분히 휴식을 취한' 청크를 우선적으로 사용하도록 개선했습니다.
    *   **예비군(Reserve) 편성**: 초기화 시 `MaxActiveChunks` 외에 추가적인 **버퍼 청크(Spare Chunks, 기본 3개)**를 미리 생성하여 풀에 넣어둠으로써, 풀이 순간적으로 비어 방금 반환된 청크를 즉시 꺼내 쓰는 상황을 원천 차단했습니다.

### 2.2 자동 간격 감지 배치 (Auto-Spacing Layout)
블루프린트(`BP_ExFloorChunk`)의 변경 사항에 유연하게 대응하기 위해 동적 배치 로직을 구현했습니다.

*   **구현 로직**:
    *   `SpawnScale` 등 하드코딩된 속성을 제거했습니다.
    *   **`GetActorBounds`** API를 활용하여, 스폰된 청크의 메쉬(Mesh)가 가진 **실제 물리적 길이(Bounds Size)**를 런타임에 정밀 측정합니다.
    *   앞선 청크의 꼬리(`MaxX`)에 다음 청크의 머리(`Extent.X`)를 정확히 맞붙이는 알고리즘을 적용하여, **스케일이나 메쉬 크기가 변경되어도 빈틈없는 배치**를 보장합니다.

### 2.3 안정적인 가시성 제어 (Robust Visibility Control)
엔진의 렌더링 스레드와 게임 로직 간의 타이밍 이슈를 해결했습니다.

*   **렌더 상태 강제 갱신**: `SetActorHiddenInGame` 호출 직후 `MarkComponentsRenderStateDirty()`를 호출하여 가시성 변경 사항이 즉시 렌더러에 반영되도록 강제했습니다.
*   **계층적 전파**: `Propagate to Children` 옵션을 활성화하여 RootComponent뿐만 아니라 하위의 모든 StaticMeshComponent까지 확실하게 숨기거나 보이도록 처리했습니다.

---

## 3. 주요 클래스 구현 상세 (Implementation Details)

### 3.1 `UExChunkSpawner` (SceneComponent)
*   **역할**: 청크의 생성, 배치, 수거, 재사용을 총괄하는 매니저입니다.
*   **주요 함수**:
    *   `InitializeSpawner()`: 기존 월드에 배치된 청크를 감지하여 리스트업하거나, 초기 트랙을 생성합니다. 예비 청크(Reserve) 생성 로직이 포함되어 있습니다.
    *   `SpawnNextChunk()`: 풀에서 청크를 꺼내(FIFO) 가장 끝 지점에 배치합니다. `GetActorBounds`를 이용한 정밀 배치 계산이 수행됩니다.
    *   `GetChunkFromPool()`: 풀에서 가장 오래된(Index 0) 청크를 반환하고 제거합니다.
    *   `ReturnChunkToPool()`: `KillZ`에 도달한 청크를 비활성화하고 풀의 끝에 추가합니다.

### 3.2 `AExFloorChunk` (Actor)
*   **역할**: 실제 바닥 역할을 하는 오브젝트입니다. 풀링 인터페이스를 지원합니다.
*   **주요 기능**:
    *   **KillZ 체크**: 매 틱(`Tick`)마다 자신의 X 좌표를 감시하여 `KillZThreshold`(-2000) 이하로 내려가면 델리게이트(`OnChunkReachedKillZ`)를 발송합니다.
    *   **가시성 관리**: `ActivateChunk` / `DeactivateChunk` 함수를 통해 물리 충돌, 틱, 렌더링 상태를 한 번에 제어합니다.

---

## 4. 트러블슈팅 및 해결 사례 (Troubleshooting Log)

### 이슈 1: 청크가 스폰 직후 사라지는 현상 (The "Instant Death" Bug)
*   **증상**: 로그상 `Activated` 직후 곧바로 `Deactivated`가 호출됨. 화면에 청크가 보이지 않음.
*   **원인**:
    1. 초기 스폰 위치(0,0,0)에서의 충돌(Collision) 발생 가능성.
    2. 풀링 로직이 LIFO여서 방금 죽은 청크를 바로 살려내려다 보니 렌더링 플래그 충돌 발생.
*   **해결**:
    *   초기 Bounds 측정 위치를 지하(`Z=-10000`)로 변경하여 충돌 회피.
    *   풀링을 FIFO로 변경하고 예비 청크를 추가하여 재사용 쿨타임 확보.

### 이슈 2: 배치 간격 불량 (Gaps and Overlaps)
*   **증상**: 청크 사이가 벌어지거나 겹침.
*   **원인**: BP에서 설정한 스케일(`Scale3D`)을 코드에서 `SpawnScale` 변수로 덮어쓰거나, 단순한 곱셈(`Length * Scale`)으로 계산하여 오차 발생.
*   **해결**: `GetActorBounds`를 도입하여 메쉬의 AABB(Axis-Aligned Bounding Box) 끝점을 기준으로 배치 좌표를 계산하도록 수정. 완벽한 칼각 배치 달성.

---

## 5. 결론 (Conclusion)
현재 `ExChunkSpawner` 시스템은 **안정성(Stability)**과 **확장성(Scalability)**을 모두 확보했습니다.
BP에서 어떤 형태의 바닥을 만들더라도 스포너는 자동으로 크기를 감지하고 끊김 없는 트랙을 생성할 준비가 되어 있습니다.

다음 단계로는 이 트랙 위를 달리는 **`ExRunnerCharacter`의 점프 및 이동 로직** 구현이 권장됩니다.
