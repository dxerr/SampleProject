# ExRunnerPlay 멀티플레이 아키텍처 개편 계획 (Multiplayer Architecture Plan)

이 문서는 다른 AI 에이전트 및 개발자가 `ExRunnerPlay`의 멀티플레이(Dedicated Server 및 Listen Server) 환경을 완벽하게 지원하기 위한 시스템 아키텍처와 마이그레이션 요구사항을 명확히 이해하고 작업할 수 있도록 작성된 기술 지침서입니다.

## 1. 아키텍처 개편 목표 (Target Environments)
- **데디케이티드 서버(Dedicated Server) 완벽 지원:** 서버는 시각적 렌더링 없이 물리 충돌 및 판정(Authority)만을 처리하며, 클라이언트들은 각자의 화면에서 부드러운 트랙을 볼 수 있어야 합니다.
- **리슨 서버(Listen Server) 충돌 방지:** 호스트가 곧 서버인 환경에서도 이중 스폰(Double Spawn)이나 트랙 클리핑 문제가 없어야 합니다.
- **대역폭 최적화:** 수많은 트랙 바닥(FloorChunk)을 일일이 네트워크로 직렬화(Replicate)하지 않고 클라이언트 로컬에서 동일하게 생성하는 '결정론적 스폰(Deterministic Spawning)'을 기본 원칙으로 합니다.

---

## 2. 핵심 변경 대상 및 설계 지침

### 2.1 결정론적 트랙 생성 (Deterministic Chunk Spawning) 
현재 무한 트랙 생성의 권한이 `GameMode`(서버 전용)에 집중되어 있어 클라이언트 환경에서는 땅이 존재하지 않아 추락합니다. 이를 각 유저의 로컬 환경에서 독립적으로, 하지만 "서로 완전히 동일하게" 스폰하도록 구조를 변경해야 합니다.

- **[스포너 이관]** `UExChunkSpawner`와 `ExObstacleManager`의 인스턴스 위치를 `AExRunnerGameMode`에서 **`AExRunnerGameState`** (또는 `UExRunnerWorldSubsystem`)로 이동.
- **[시드 동기화]** 서버의 `GameMode` 시작 시 생성된 `RandomStream(Seed)`을 `GameState`에 담아 `Replicated`로 모든 클라이언트에게 전송.
- **[로컬 스폰 실행]** 데디/리슨 서버 및 모든 클라이언트는 전달받은 동일한 Seed를 기반으로 각자의 오프라인 `ChunkSpawner`를 가동하여 자기 월드에 `AExFloorChunk(bReplicates=false)`를 배치.

### 2.2 다중 플레이어 거리 동기화 (Multi-player Distance Synchronization)
현재는 `Player 0` 한 명의 위치만 측정하여 트랙 진행을 결정합니다. 멀티플레이에서는 1등을 기준으로 트랙을 깔고, 꼴등을 기준으로 뒤쪽 트랙을 지워야 합니다.

- **[선두/후행 판별 시스템]**
  - `AExRunnerGameState::Tick`에서 월드 내 존재하는 모든 **`AExRunnerCharacter` (또는 활성 폰)** 들의 PathDistance를 갱신 및 배열 정렬합니다.
  - `LeadDistance` (1등의 이동 거리)와 `TailDistance` (꼴등의 이동 거리)를 산출하여 Replicated 변수로 갱신.
- **[스폰 기준점 변경]**
  - `ExChunkSpawner`는 기존처럼 특정 유저가 아닌, `GameState->LeadDistance`를 기준으로 앞에 트랙을 이어붙입니다.
  - 바닥 삭제(KillZ) 판정은 `GameState->TailDistance - KillZ_Offset`을 기준으로 하여 뒤쳐진 플레이어가 발판이 없어 떨어지는 일을 방지합니다.

### 2.3 액터 리플리케이션 전략 분리 (Local vs Replicated)
멀티플레이에서 최적화를 위해 시각적 배경과, 게임플레이 핵심 기믹 간의 동기화 전략을 분리해야 합니다.

*   **AExFloorChunk (바닥/배경)**
    *   `bReplicates = false` (현재 유지).
    *   서버와 각 클라이언트가 독립적으로 스폰하며 Attachment를 사용해 엮습니다.
*   **고정 장애물 (Static Obstacles)**
    *   바닥에 고정된 움직이지 않는 벽 등은 `bReplicates = false`로 두고 FloorChunk와 함께 로컬 스폰.
*   **동적/상호작용 장애물 및 아이템 (Dynamic Obstacles & Coins/Items)**
    *   움직이거나(예: 좌우 롤러), 한 명이 먹으면 사라져야 하는 아이템은 어설프게 로컬 생성하면 상태 동기화(누가 먹었나?)가 매우 복잡해집니다.
    *   **권장 설계:** `bReplicates = true`. 
    *   서버에서만 직접 해당 액터들을 스폰하고, 클라이언트에게 리플리케이션 방식으로 전달.
    *   주의: 바닥(FloorChunk)이 클라이언트 로컬(NetGUID 없음)이므로, 서버가 생성한 리플리케이션 아이템을 바닥에 `Attach`하면 클라이언트에서 위치 깨짐이 발생할 수 있습니다. 스폰 시 Attachment 대신 **World Parameter(절대 좌표)** 로 고정 스폰해야 합니다.

### 2.4 네트워크 이동 및 판정 (Mover 2.0 기반)
*   움직임 관련 조향(`MergedInput` / `OrientationIntent`)은 현재 `UExRunnerMovementComponent`와 `Mover 2.0`의 클라이언트 예측 프레임워크를 그대로 타도록 둡니다.
*   데미지(장애물 충돌) 및 결승선(Goal) 판정과 같은 치명적인 트리거는 **오직 서버(Server Authority)**에서만 판정하며, `ExGameplayEvent`를 통해 RPC로 클라이언트에게 데미지 연출/효과만 방송(Multicast)합니다.

---

## 3. 마이그레이션 1차 수행 스펙 (Phase 8 Checklist)
- [ ] `AExRunnerGameState`에 공유될 `SharedTrackSeed` (int32, Replicated) 변수 선언.
- [ ] `AExRunnerGameState`에 `LeadPathDistance` / `TailPathDistance` Replicated 추적 로직 구성.
- [ ] `AExRunnerGameMode`에 있던 `ChunkSpawner`, `ObstacleManager`, `ItemManager` 선언부 및 초기화 로직을 `AExRunnerGameState` 내부로 이관.
- [ ] 로컬 스포닝 중복 에러 해결(아이템 리플리케이션 아키텍처: World Space Spawn) 전환 작업.

> 위 사항들을 모두 반영하면, 접속한 플레이어 모두가 동일하게 휘어지는 트랙을 공유받으며, 1등을 중심으로 계속해서 트랙이 뻗어나가는 완벽한 멀티플레이 Runner 환경이 완성됩니다.
