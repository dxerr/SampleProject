# 장애물 및 아이템 클라이언트 동기화 해결 계획

> **버전:** v2.2 (Replicated→Non-Replicated Attach 위험 해소 — 수동 Attach 모드 채택)
> **작성일:** 2026-05-07
> **개정 이력:**
> - v1.0: 결정론적 로컬 스폰(A안)
> - v2.0: C안 — 하이브리드 결정론 채택
> - v2.1: 외부 AI 검토 의견 6건 분석 후 4건 반영 (초기화 순서 계약, Attach 모드 분리, Item Plan의 결정론 입력원, 매핑표 정합성)
> - **v2.2: 추가 외부 검토 — UE 엔진 레벨 Attach Replication의 NetGUID 의존성 확인 후 모드 A 폐기, "Manual Re-Attach 모드" 채택 (치명 결함 해소). 누적 상태값 드리프트 명기. Plan 정렬 보장.**
> **대상 엔진:** Unreal Engine 5
> **프로젝트:** ExFrameWork / ExRunnerPlay
> **의존 문서:**
> - `Md/Architecture/ExRunnerPlay/Multiplayer_Network_Readiness_Analysis.md`
> - `Md/Architecture/ExRunnerPlay/ExFrameWork_Item_System_Architecture.md` (v1.2)
> - `Md/Architecture/ExRunnerPlay/ExRunner_Obstacle_System_Architecture.md`
> - `Md/Bug/MultiPlay/Bug_Mover_Multiplayer_Sync_Error.md`

---

## 1. 상황 분석 (Context)

### 1.1 현상

멀티플레이 환경에서 **장애물(Obstacle)** 과 **아이템(Coin/Buff)** 이 서버(Host) 화면에는 보이지만 클라이언트 화면에는 나타나지 않음.

### 1.2 원인

현 코드는 GameState 이관·시드 동기화·PathManager의 `FRandomStream` 도입까지 완료되어 트랙(FloorChunk)은 정상 동기화되고 있다. 그러나 **장애물·아이템 매니저**에서만 다음 문제가 남아 있다.

| ID | 위치 | 문제 |
|---|---|---|
| O-1 | `UExObstacleManager::SpawnObstaclesOnChunk` 진입부 | `if (GetOwner() && !GetOwner()->HasAuthority()) return;` — 클라이언트 차단 |
| O-2 | `UExObstacleManager::SpawnObstaclesOnChunk` 본문 | `FMath::FRand()`, `FMath::RandRange()` 사용 — 서버·클라 결과 불일치 |
| O-3 | `UExObstacleManager` 장애물 Attach | `AttachToActor(Chunk, KeepWorldTransform)` — Chunk가 `bReplicates=false`인 로컬 액터라 리플리케이트되는 장애물은 NetGUID 매핑 불가 위험 |
| I-1 | `UExRunnerItemManager::SpawnItemsOnChunk` 진입부 | 동일한 Authority 가드로 클라이언트 차단 |
| I-2 | `UExRunnerItemManager::SpawnCoinLine` / `SpawnBuffItem` | 다수의 `FMath::FRand()`, `FMath::RandRange()` — 결정론 미보장 |
| I-3 | `UExRunnerItemManager::CalculateItemZ` | Slide 분기에서 `FMath::FRand() < SlideTopPlacementRatio` — 비결정론 |
| I-4 | 코인 Attach | `AttachToActor(Chunk, KeepWorldTransform)` — 동일 위험 |

### 1.3 설계 충돌 정리

기존 두 문서가 서로 다른 방향을 제시하여 통일이 필요했다.

- **본 계획서 v1.0**: "결정론적 로컬 스폰" — 모든 장애물·아이템을 클라이언트 각자 스폰, `bReplicates=false`.
- **`Multiplayer_Network_Readiness_Analysis.md` §2**: "동적/상호작용 장애물·아이템은 서버 전용 + Replicated".

---

## 2. 채택 방안: C안 — 하이브리드 결정론 (Deterministic Layout + Authoritative Collision)

### 2.1 채택 사유

순수 결정론 로컬 스폰(A안)은 FloorChunk처럼 "보기에만 같으면 되는" 액터에는 완벽하지만, **장애물은 캐릭터 물리·상호작용 시뮬레이션에 직접 관여**한다는 점에서 본질이 다르다.

A안 채택 시 발생할 위험:

1. **Climb/Vault 파쿠르 desync**: 서버·클라 각자의 장애물 액터에 대해 Overlap을 별도 판정 → 캐릭터 위치가 1cm라도 어긋나면 한쪽만 트리거.
2. **Gap 점프 추락 desync**: 바닥 충돌체가 클라이언트별로 별개 인스턴스. Mover의 클라이언트 예측이 서버와 1프레임 어긋나면 추락 결과 불일치 (이전 `Bug_Mover_Multiplayer_Sync_Error.md`와 동일 패턴).
3. **픽업 우선권 분쟁**: 서버·클라가 서로 다른 액터를 Overlap → "누가 먼저 먹었나" 판정 RPC 복잡도 폭증.

### 2.2 C안의 핵심 아이디어

**레이아웃 결정**(어디에 무엇을 둘지)은 결정론적 시드로 서버·클라가 각자 산출하지만, **액터 인스턴스의 실체**는 서버 스폰 후 표준 리플리케이션으로 통일.

```
┌─────────────────────────────┬─────────────────────────────┐
│ Step                        │ 주체                        │
├─────────────────────────────┼─────────────────────────────┤
│ 1. 레이아웃 산출             │ 서버 + 클라 (각자, 결정론)   │
│    → FExSpawnPlan[] 배열    │                             │
├─────────────────────────────┼─────────────────────────────┤
│ 2. (선택) 해시 검증          │ 클라 (디버그 빌드 한정)      │
├─────────────────────────────┼─────────────────────────────┤
│ 3. 액터 스폰                 │ 서버 전용                    │
│    → SpawnActor(bReplicates)│                             │
├─────────────────────────────┼─────────────────────────────┤
│ 4. 클라 수신                 │ 자동 (엔진 리플리케이션)     │
├─────────────────────────────┼─────────────────────────────┤
│ 5. Pickup / 충돌 / 파쿠르   │ 서버 권한 (단일 인스턴스)    │
└─────────────────────────────┴─────────────────────────────┘
```

### 2.3 통일된 동기화 패턴

ExRunnerPlay의 모든 스폰 시스템이 **동일한 결정론 골격**을 따르게 된다. ChunkSpawner와의 일관성이 핵심이다.

| 시스템 | 레이아웃 산출 (Seed) | 실체화 | bReplicates |
|---|---|---|---|
| **PathManager 세그먼트** | `PathRandomStream` (양측) | 데이터 구조 (액터 아님) | — |
| **FloorChunk** | 세그먼트 데이터 (양측) | 양측 로컬 SpawnActor | `false` |
| **장애물 (C안)** | `ObstacleRandomStream` (양측) | **서버만 SpawnActor** | `true` |
| **아이템 (C안)** | `ItemRandomStream` (양측) | **서버만 SpawnActor** | `true` |

→ 모든 스폰이 "Seed → Plan → Spawn"이라는 동일한 3단 구조로 통일됨.

---

## 3. 핵심 컴포넌트 설계

### 3.1 FExSpawnPlan 구조체 (신규)

장애물·아이템의 "배치 의도"를 표현하는 가벼운 데이터 구조체. 액터가 아닌 순수 데이터이므로 클라가 산출하더라도 비용이 거의 없다.

**역할:**
- 서버: `RealizeObstaclePlan()`이 이 배열을 순회하며 실제 액터를 SpawnActor.
- 클라: 산출만 하고 실체화는 하지 않음. 다만 (a) 디버그 해시 검증, (b) JIP catch-up용 자료, (c) 미래 시각 힌트(다가오는 장애물 미리보기) 용도.

**필드 (의도만 기술, 코드 예시는 의도적으로 생략):**
- 어떤 청크에 속하는지 식별자 (PathDistance 또는 SegmentIndex)
- 청크 내 로컬 거리(Offset)
- 어떤 Definition으로 스폰할지 (소프트 레퍼런스 또는 인덱스)
- 부가 파라미터 (LaneIndex, LateralOffset, ScaleHint 등 전략별 필요값)
- 결정론 체크용 해시 시드 (선택)

**배치 위치:** `ExCore` 모듈의 `Struct/Items/FExSpawnPlan.h` (장애물·아이템 공용)

### 3.1.1 초기화 순서 계약 (Initialization Contract)

결정론의 출발점이므로 호출 순서를 강제 규약으로 명시한다. 한 단계라도 어긋나면 시드 0으로 동작하여 양측 결과가 맞아 보이는 우연한 일치만 발생할 수 있어 발견이 늦어진다.

**규약 — 서버 (Authority):**

1. `GameMode::StartRunnerGame` → `GS->SharedTrackSeed = FMath::Rand()` (시드 확정)
2. `GS->PathManager->InitializePath(...)` 내부에서 `PathRandomStream.Initialize(SharedTrackSeed)`
3. **`GS->ObstacleManager->InitializeRandomStream(SharedTrackSeed)`** (신규 — Phase 1 추가 필수)
4. **`GS->ItemManager->InitializeRandomStream(SharedTrackSeed)`** (신규 — Phase 1 추가 필수)
5. `GS->ChunkSpawner->InitializeSpawner()` → 첫 청크 스폰 → `Generate*Plan()` 호출 시작

**규약 — 클라이언트:**

1. `OnRep_SharedTrackSeed` 진입
2. `PathManager->InitializePath(...)` (기존 로직)
3. **`ObstacleManager->InitializeRandomStream(SharedTrackSeed)`** (신규)
4. **`ItemManager->InitializeRandomStream(SharedTrackSeed)`** (신규)
5. `ChunkSpawner->InitializeSpawner()` → `SetManagers` → `BindToSpawner` (기존 흐름)

**위반 감지:**

- `Generate*Plan()` 진입부에 `ensureMsgf(bRandomStreamInitialized, ...)` 추가.
- 스트림 미초기화 상태에서 호출되면 디버그 빌드에서 즉시 인지 가능.

**문서화 위치:** `OnRep_SharedTrackSeed` 함수 상단 주석 + 각 매니저의 `InitializeRandomStream` 함수 주석에 본 계약 명기.

### 3.2 RandomStream 분리 (양측 동일하게 동작)

`AExRunnerGameState::SharedTrackSeed`를 기반으로 **시스템별 독립 스트림**을 파생한다. 이미 `PathRandomStream`이 있는 패턴을 그대로 확장.

| 스트림 | 보관 위치 | Seed 파생 식 |
|---|---|---|
| `PathRandomStream` (기존) | `UExPathManager` | `Hash(SharedTrackSeed, 0)` |
| `ObstacleRandomStream` (신규) | `UExObstacleManager` | `Hash(SharedTrackSeed, 1)` |
| `ItemRandomStream` (신규) | `UExRunnerItemManager` | `Hash(SharedTrackSeed, 2)` |

**추가 안전장치 (선택):** 각 스트림은 청크 단위로 재시드 (`Stream.Initialize(Hash(BaseSeed, SegmentIndex))`)하여 한 청크에서의 소비 횟수가 어긋나도 다음 청크에서 자동 복구되도록 설계 가능. 1차 구현은 단일 스트림으로 시작하고 드리프트 발견 시 도입.

### 3.3 ObstacleManager 개편

**핵심 변경:**
- `SpawnObstaclesOnChunk`를 두 단계로 분리:
  - **Stage 1 — Plan**: `GenerateObstaclePlan(Chunk)` → `TArray<FExSpawnPlan>` 반환. **Authority 가드 없음.** 양측 모두 호출. `ObstacleRandomStream`만 사용.
  - **Stage 2 — Realize**: `RealizeObstaclePlan(Plan, Chunk)` → 서버에서만 SpawnActor. **`ensureMsgf(HasAuthority())` 가드 필수.**
- 클라이언트는 Plan 산출 후 액터 생성 없이 종료. 액터는 서버가 만들어 자동 리플리케이트.

**Attach 정책 — UE 엔진 동작 검증 후 결정 (v2.2 갱신):**

**v2.1의 모드 A는 폐기**한다. UE의 표준 Attach Replication은 다음과 같이 동작하므로 Replicated→Non-Replicated 부모 Attach는 작동하지 않는다 (UE 5 공식 문서 및 엔진 소스 검증):

- 서버에서 Replicated Actor가 다른 Actor에 `AttachToActor`되면 엔진이 `FRepAttachment` 구조체에 부모의 NetGUID를 담아 전송.
- **부모가 Replicated가 아니면 NetGUID가 없으므로 클라이언트는 부모를 식별 불가** → Attach가 풀린 채 잘못된 위치(원점 또는 미해결 좌표)에 액터가 나타남.
- 또한 공식 문서: "AttachParent가 non-nullptr인 동안에는 movement replication이 발생하지 않는다" — Attach가 한 번이라도 시도되면 위치 갱신마저 멈춰 상황이 더 악화될 수 있음.

→ FloorChunk가 `bReplicates=false`인 한, **엔진 자동 Attach Replication은 사용할 수 없다.**

**채택안 — Manual Re-Attach 모드:**

서버는 Attach를 걸지 않고 World Space로 스폰한다. 대신 어느 청크에 속하는지를 **Replicated 변수로 명시**하여, 클라이언트가 OnRep에서 자기 로컬 청크를 찾아 **수동으로 Attach**한다.

핵심 설계:

1. **`AExItemPickupBase` / 장애물 액터에 Replicated 식별자 추가:**
   - `UPROPERTY(ReplicatedUsing=OnRep_OwnerChunkKey) float OwnerChunkPathDistance` — 부모 청크의 PathDistance (청크별 고유값)
   - 또는 `int32 OwnerSegmentIndex` — 정수 키가 더 안전 (부동소수 비교 회피)
   - 1차에서는 **`SegmentIndex`** 사용 권장 (해시 안정성)

2. **서버 측 `Realize*Plan`:**
   - `SpawnActor`로 World Space 스폰 (Attach 호출하지 않음)
   - 스폰 직후 `Spawned->OwnerSegmentIndex = Chunk->SegmentIndex` 설정 → Replicated로 자동 전파

3. **클라이언트 측 `OnRep_OwnerChunkKey`:**
   - 자기 매니저(GameState 컴포넌트)에서 `SegmentIndex`로 로컬 청크를 조회
   - 찾으면 `AttachToActor(LocalChunk, FAttachmentTransformRules::KeepWorldTransform)` **수동 호출**
   - 못 찾으면(아직 청크가 도착 안 함) 매니저의 "대기 큐"에 등록 → 이후 청크 스폰 시점에 재시도

4. **NetUpdateFrequency 영향 회피:**
   - 위치는 SpawnActor 시점의 World Transform에 이미 정확히 박혀 있고, 이후 청크가 시프트되지 않는 ExRunnerPlay 구조에서는 위치 갱신이 불필요.
   - Attach는 **시각적 부모-자식 관계**(에디터 디버깅, 일괄 회수)를 위한 수단일 뿐, 위치 동기화 수단이 아님.

5. **Despawn 회수 — 양측 인덱스 기반:**
   - 매니저는 `TMap<int32 SegmentIndex, TArray<TWeakObjectPtr<AActor>>> SpawnedBySegment` 인덱스를 양측에서 유지.
   - 청크 Despawn 이벤트 수신 시 해당 SegmentIndex로 자기 매니저의 인덱스에서 액터들을 풀로 회수.
   - 서버에서는 풀 반환 시 `SetActorHiddenInGame(true)` 등으로 비활성화 → 자동으로 Replicated 종료 처리.
   - 클라에서는 자기 인덱스에서 제거만 하면 되며, 실제 액터 파괴는 서버 리플리케이션 종료가 담당.

6. **Query 메커니즘 — Plan 기반 (이미 §3.4에 정립):**
   - `QueryObstacleAtDistance`는 더 이상 `GetAttachedActors()` 의존이 불가능 (Attach 시점이 OnRep 후 비동기).
   - 대신 §3.4의 `QueryObstaclePlanAtDistance(Plan, ...)`이 결정론 경로의 표준 질의 함수.
   - 게임플레이 런타임 질의(파쿠르 판정 등 Authority 컨텍스트)는 위 인덱스(`SpawnedBySegment`)를 활용하여 거리 기반 검색.

**시각적 부착 검증 항목 (Phase 5):**

- 서버에서 청크가 KillZ로 사라질 때 클라이언트 측 장애물·아이템도 같이 사라지는가? (인덱스 기반 회수 정상)
- 클라이언트에 청크가 늦게 도착해도 OnRep의 대기 큐 처리로 정확한 Attach가 이루어지는가?
- 청크와 함께 시각적으로 묶여 있는 인상이 유지되는가? (현재 트레드밀 구조에서는 청크가 시프트되지 않으므로 World Space 좌표 그대로도 시각적 정합성 유지)

**현재 구조와의 적합성:**

ExRunnerPlay는 **트레드밀 구조** — 청크는 World Space에서 고정, 캐릭터가 진행 거리를 누적하는 방식. 즉 청크가 "물리적으로 이동"하지 않으므로 World Space 절대좌표 스폰만으로도 시각적 정합성이 유지됨. Attach는 회수 일괄 처리 및 디버그 가시성을 위한 보조 수단. 향후 청크가 동적으로 시프트되는 구조로 변경된다면 Attach에 Movement Replication 의존이 발생하므로 이 설계 전제가 재검토되어야 함 (현재로선 해당 없음).

**KillZ 판정의 독립성 (Manual Re-Attach 모드 공통):**

- 장애물·아이템의 KillZ 회수 트리거는 **부모 청크의 Despawn 시점**에 동기화 (Chunk가 사라지면 매니저가 SegmentIndex 인덱스로 일괄 회수).
- 장애물·아이템 자체는 독립적인 Tick 기반 KillZ 판정을 하지 않음 → 부모 청크 회수 이벤트로 단일화.

### 3.4 RunnerItemManager 개편

ObstacleManager와 **완전히 동일한 패턴** 적용:

- `SpawnItemsOnChunk`를 `GenerateItemPlan` + `RealizeItemPlan`으로 분리.
- `ItemRandomStream` 도입.
- `CalculateItemZ` 내부의 `FMath::FRand() < SlideTopPlacementRatio` → `ItemRandomStream.FRand() < SlideTopPlacementRatio`로 교체.
- **뱀 패턴 상태값**(`PersistentTargetLane`, `RemainingCoinsInCurrentLane`, `CurrentLaneYOffset`, `PersistentNextCoinDistance`)은 양측이 동일하게 갱신되어야 결정론이 유지됨. → Plan 생성 단계에서 갱신, 실체화 단계는 갱신하지 않음.
- 코인 Attach 정책은 ObstacleManager와 동일하게 §3.3 Manual Re-Attach 모드를 따름.

**누적 상태값 드리프트 위험 — 명시 (v2.2 신설):**

위 상태값들은 **청크를 가로질러 누적**되는 변수다. 1차 구현에서는 다음 가정에 의존한다:

- 서버와 클라이언트가 **동일한 시점에 동일한 청크 인덱스 순서로** `GenerateItemPlan`을 호출한다.
- ChunkSpawner가 양측에서 결정론적으로 호출되며, 호출 횟수가 한 번도 어긋나지 않는다.

이 가정은 §3.1.1 초기화 계약과 ChunkSpawner의 결정론으로 1차에서는 보장된다. 그러나 다음 시나리오에서 깨질 위험이 있다:

- 클라이언트의 일시적 지연 로딩 또는 패킷 손실로 청크 생성 호출이 어긋나는 경우
- JIP 클라이언트가 중간에 합류하여 청크 인덱스 N부터 생성을 시작하는 경우 (2차 작업 — §7.1)
- 향후 디자인 변경으로 청크 생성 순서가 비결정적이 되는 경우

**완화 방안 (단계적):**

- **1차 (현재)**: 호출 순서 정합성에 의존. 매니저 진입부에 `ensureMsgf(ExpectedSegmentIndex == Chunk->SegmentIndex, ...)` 어설트로 호출 순서 위반을 즉시 감지.
- **2차 (Stateless 전환)**: 청크 인덱스 또는 PathDistance를 입력으로 하는 Stateless 함수로 뱀 패턴을 재설계. 예: `CalcSnakeStateAtSegment(SegmentIndex)` → 호출 누락이 있어도 자동 복구. 이는 §7.3 청크별 재시드와 함께 진행하여 통합적으로 해결.

**Plan 배열 정렬 보장 (v2.2 신설):**

`GenerateObstaclePlan`과 `GenerateItemPlan`이 반환하는 `TArray<FExSpawnPlan>`은 반드시 **`PathDistance` 오름차순으로 정렬된 상태**로 산출한다. 현재 청크당 항목 수가 많지 않으므로(장애물 1~3, 코인 ~20) 1차 구현은 선형 탐색으로 충분하지만, 정렬 불변식을 유지함으로써:

- 향후 청크당 항목이 늘어났을 때 `QueryObstaclePlanAtDistance`에서 이진 탐색(`Algo::LowerBound`)으로 즉시 전환 가능.
- 디버그 시각화·로깅에서 거리 순 출력이 자연스러움.
- Plan 해시 비교(2차) 시 양측이 같은 순서로 직렬화하므로 해시 안정성 확보.

자연 산출 과정상 거리 순으로 만들어지므로 별도 정렬 비용은 거의 없으며, 산출 직후 `ensure(IsSortedByDistance(Plan))` 수준의 가드만 두면 충분하다.

**Plan 기반 Query — 결정론 입력원의 단일화 (필수):**

현재 `QueryObstacleAtDistance`는 **Attach된 실액터**를 순회하여 장애물 컨텍스트를 산출한다(`ExObstacleManager.cpp` L383~ 코드 검증 완료). C안에서 클라이언트는 `RealizeObstaclePlan`을 호출하지 않으므로 클라에는 실액터가 없다 → **클라의 `GenerateItemPlan`이 기존 `QueryObstacleAtDistance`를 호출하면 빈 결과**를 받게 되어 동일 시드라도 서버는 "Climb 위에 코인" 산출, 클라는 "장애물 없음으로 바닥에 코인" 산출 → **결정론 깨짐**.

따라서 다음 규칙을 강제한다:

1. **`GenerateItemPlan`은 실액터 조회를 사용하지 않는다.** 대신 동일 청크에 대해 직전 단계로 산출된 `GenerateObstaclePlan` 결과(`TArray<FExSpawnPlan>`)를 입력으로 받는다.
2. ObstacleManager는 **Plan 기반 질의 함수** `QueryObstaclePlanAtDistance(const TArray<FExSpawnPlan>& Plan, float PathDist, float QueryRadius, FExObstacleContext& OutContext) const`를 신규 제공한다.
3. 기존 실액터 기반 `QueryObstacleAtDistance`는 **런타임 게임플레이 코드(파쿠르 판정 등 서버 권한 컨텍스트)에서만** 사용. 양측에서 호출되는 결정론 경로(`GenerateItemPlan`)에서는 호출 금지.
4. ChunkSpawner의 호출 흐름은 **반드시 ObstaclePlan을 먼저 산출한 뒤 그 결과를 ItemManager에 전달**해야 한다 (§3.5 참조).

**서버 권한 가드 — `if + ensure` 이중 안전장치:**

`UExItemSpawnManagerBase::SpawnItem`은 현재 `BlueprintAuthorityOnly`만 있고 C++ 런타임 가드가 없어, C++ 코드에서 직접 호출되면 클라이언트에서도 실행된다. ensure 단독 사용은 로그만 남기고 실행은 계속되어 액터 스폰을 차단하지 못하므로, **다음 두 줄 패턴**을 표준으로 한다:

```
// 함수 진입부 (의도 표현, 실제 코드는 구현 시점에 작성)
if (!HasAuthority())
{
    ensureMsgfAlways(false, TEXT("[ExItemSpawnManager] SpawnItem이 클라이언트에서 호출됨"));
    return nullptr;
}
```

- `if (!HasAuthority()) return`: **차단** 역할. 클라에서 잘못 호출되어도 액터가 실제로 안 만들어짐.
- `ensureMsgfAlways`: **알림** 역할. 디버그 빌드에서 호출 사이트를 즉시 인지.
- 같은 패턴을 `RealizeObstaclePlan`, `RealizeItemPlan`에도 적용.
- `checkf`는 Shipping 빌드에서 크래시까지 유발하므로 본 경로에는 사용하지 않는다 (러너 게임 본 경로의 안정성 우선).

### 3.5 ChunkSpawner 호출 흐름 변경

기존: `SpawnNextChunk` 마지막에 `SpawnObstaclesOnChunk` → `SpawnItemsOnChunk` 직접 호출.

신규: 동일 위치에서 다음 순서로 호출.

1. `const TArray<FExSpawnPlan> ObstaclePlan = ObstacleManager->GenerateObstaclePlan(Chunk)` — 양측 (결정론 산출)
2. `const TArray<FExSpawnPlan> ItemPlan = ItemManager->GenerateItemPlan(Chunk, ObstaclePlan)` — 양측. **Plan을 직접 전달하여 클라이언트도 장애물 정보를 알 수 있게 한다 (§3.4 Plan 기반 Query 규칙).**
3. `if (HasAuthority())` 분기 안에서:
   - `ObstacleManager->RealizeObstaclePlan(ObstaclePlan, Chunk)`
   - `ItemManager->RealizeItemPlan(ItemPlan, Chunk)`

**Plan 보관:** Plan 배열은 Chunk별로 매니저 내부 `TMap<TWeakObjectPtr<AExFloorChunk>, TArray<FExSpawnPlan>>`에 저장. Chunk Despawn 시 정리. 매니저는 또한 클라이언트 측 Plan도 보관하여 향후 JIP catch-up 또는 디버그 검증에서 재활용.

---

## 4. 결정론 보장 체크리스트

| 항목 | 보장 방법 |
|---|---|
| 시드 동기화 | `SharedTrackSeed` Replicated, `OnRep_SharedTrackSeed`에서 PathManager·ChunkSpawner·매니저들 일괄 초기화 |
| 청크 생성 결정론 | `PathRandomStream` (이미 적용됨) |
| 장애물 배치 결정론 | `ObstacleRandomStream` 신규 도입, 모든 비결정론 호출 교체 |
| 아이템 배치 결정론 | `ItemRandomStream` 신규 도입, 모든 비결정론 호출 교체 |
| 조건부 분기 드리프트 | (1차) 단일 스트림, (2차) 청크별 재시드로 자동 복구 |
| Plan 산출 시점 일치 | ChunkSpawner가 Generate 호출을 명시적 동기 순서로 호출하므로 보장 |
| 검증 (디버그) | `FExSpawnPlan` 해시를 양측이 비교하는 디버그 RPC (선택, 2차) |

---

## 5. 판정 동기화 안전성 분석

C안이 A안(순수 로컬)에 비해 갖는 본질적 안전성을 명시한다.

| 시나리오 | A안 (순수 로컬) | **C안 (채택안)** |
|---|---|---|
| Climb/Vault 파쿠르 Overlap | 양측 액터 별개 → 위치 1cm 차이로 desync 위험 | 서버 액터 단일 인스턴스 → 권위적 단일 판정 ✅ |
| Gap 추락 충돌 | 양측 별개 충돌체 → Mover 예측 desync 위험 | 서버 단일 충돌체 → Mover와 동일 권위 모델 ✅ |
| 코인 픽업 우선권 | "누가 먼저"를 RPC로 별도 조정 필요 | 서버 Overlap = 단일 진실 ✅ |
| 시각적 위치 일치 | Seed 보장 (단, 액터 정체성 다름) | Seed 보장 + 액터도 동일 인스턴스 ✅ |
| 네트워크 비용 | 0 (Replicate 안 함) | 1회 초기 Replicate (액터 생성 시) |

→ 네트워크 비용은 약간 증가하지만, **물리·상호작용 권위가 단일화되어 desync 가능성이 원천 차단**됨. 모바일 러너 환경에서도 청크당 장애물 1~3개 + 코인 ~20개 수준이라 부담 없음.

---

## 6. 구현 단계 (Phased Implementation)

### Phase 1 — 인프라 (FExSpawnPlan + Stream 분리 + 초기화 계약)

1. `FExSpawnPlan` 구조체 정의 (`ExCore/Struct/Items/`)
2. `UExObstacleManager`에 `ObstacleRandomStream` + `bRandomStreamInitialized` 플래그 추가
3. `UExRunnerItemManager`에 `ItemRandomStream` + 동일 플래그 추가
4. **`InitializeRandomStream(int32 SharedSeed)` 신규 메서드 추가** (양 매니저)
5. **`AExRunnerGameState::OnRep_SharedTrackSeed` 및 `AExRunnerGameMode::StartRunnerGame` 양쪽에 §3.1.1 초기화 계약 순서대로 호출 추가** — 양측의 호출 순서가 동일해야 함
6. `Generate*Plan()` 진입부에 `ensureMsgf(bRandomStreamInitialized, ...)` 가드 추가

### Phase 2 — ObstacleManager 분리

1. `GenerateObstaclePlan(Chunk)` 신규 — Authority 가드 없음, `ObstacleRandomStream` 사용
2. `RealizeObstaclePlan(Plan, Chunk)` 신규 — `if (!HasAuthority()) { ensure...; return; }` 이중 가드, 실 SpawnActor
3. `QueryObstaclePlanAtDistance(Plan, ...)` 신규 — Plan 기반 질의 함수 (§3.4 규칙)
4. 기존 `SpawnObstaclesOnChunk` 제거 또는 두 단계 호출 래퍼로 변경
5. ChunkSpawner에서 호출 순서 갱신 (§3.5)
6. 기존 `QueryObstacleAtDistance`는 게임플레이 코드 전용으로 명시 주석 추가

### Phase 3 — RunnerItemManager 분리

1. `GenerateItemPlan(Chunk, const TArray<FExSpawnPlan>& ObstaclePlan)` 신규 — **ObstaclePlan을 파라미터로 받아** Plan 기반 질의 사용 (§3.4 규칙). 뱀 패턴 상태값 갱신 포함
2. `RealizeItemPlan(Plan, Chunk)` 신규 — `if (!HasAuthority()) { ensure...; return; }` 이중 가드, 서버 전용 SpawnActor
3. `CalculateItemZ`의 `FMath::FRand()` 교체
4. `UExItemSpawnManagerBase::SpawnItem` 진입부에 §3.4의 `if + ensure` 이중 가드 표준 패턴 적용
5. ChunkSpawner에서 호출 순서 갱신 (§3.5)

### Phase 4 — Manual Re-Attach 구현 (v2.2 변경)

1. `AExItemPickupBase` 및 장애물 액터 클래스에 **`UPROPERTY(ReplicatedUsing=OnRep_OwnerChunkKey) int32 OwnerSegmentIndex`** 추가.
2. `GetLifetimeReplicatedProps`에 등록.
3. `OnRep_OwnerChunkKey` 구현: GameState 매니저를 통해 로컬 SegmentIndex 청크 조회 → `AttachToActor(LocalChunk, KeepWorldTransform)` 수동 호출. 청크 미도착 시 매니저의 대기 큐에 등록.
4. 매니저(`UExObstacleManager`, `UExRunnerItemManager`)에 다음 추가:
   - `TMap<int32, TArray<TWeakObjectPtr<AActor>>> SpawnedBySegment` (양측 인덱스)
   - `TArray<TWeakObjectPtr<AActor>> PendingAttachQueue` (클라 전용 대기 큐)
   - `OnLocalChunkSpawned(AExFloorChunk*)` 이벤트 수신 시 대기 큐에서 매칭되는 액터 Re-Attach 시도.
5. `RealizeObstaclePlan` / `RealizeItemPlan`은 **`AttachToActor` 호출 제거**, 대신 `OwnerSegmentIndex` 설정 후 `SpawnedBySegment` 인덱스에 등록.
6. `OnChunkDespawned` 핸들러를 `GetAttachedActors()` 기반에서 **`SpawnedBySegment[Chunk->SegmentIndex]` 인덱스 기반**으로 재작성.
7. `AExFloorChunk`에 `int32 SegmentIndex` 멤버 추가 (없다면), 청크 활성화 시 ChunkSpawner가 PathManager의 세그먼트 인덱스를 청크에 기록.

### Phase 5 — 검증 및 디버깅

**기능 검증 (필수):**

1. 멀티 PIE (서버 + 1 클라) 테스트: 양측 동일 위치에 장애물·아이템 출현 확인
2. **Manual Re-Attach 동작 검증** — 클라이언트에서 장애물·아이템 OnRep 시 SegmentIndex 매칭 청크에 정상 Attach되는지, 청크 도착 지연 시 대기 큐가 정상 동작하는지 확인.
3. **청크 Despawn 시 일괄 회수 검증** — `SpawnedBySegment` 인덱스 기반 회수가 양측에서 동기화되는지, 누락 액터가 없는지 확인.
4. 픽업·파쿠르 정상 동작 확인 (Climb/Vault 모션 워핑이 서버·클라 양쪽에서 일관되게 발동)
5. **호출 순서 정합성 어설트 작동 확인** — 매니저 진입부의 `ensureMsgf(ExpectedSegmentIndex == Chunk->SegmentIndex)`가 정상 시나리오에서 발생하지 않는지, 의도적 지연 테스트에서 정상 감지되는지 확인.

**경량 네트워크 튜닝 (1차 권장):**

6. `AExItemPickupBase::bAlwaysRelevant = false`로 변경 검토 — 현재 `true`로 모든 클라이언트에 항상 관련 액터로 처리되어 부담 가능성. 청크 단위 거리 기반 Relevancy로 충분.
7. `AExItemPickupBase::NetUpdateFrequency`를 기본값(100Hz)에서 5~10Hz 수준으로 하향 — 픽업은 한 번 동기화되면 위치가 변하지 않으므로 고빈도 불필요.
8. (선택) `bNetUseOwnerRelevancy` 또는 Network Dormancy 설정 검토.

> **주의**: 위 튜닝은 측정 없이 단정적으로 수치를 결정하지 않는다. 1차 PIE 테스트에서 Network Profiler로 실측 후 적용 여부를 결정. 정량 KPI(청크당 스폰 수, 초당 Replicate 바이트, 타겟 동접 등) 정밀 측정은 2차 작업으로 미룸.

**(선택) 결정론 자동 검증:**

9. 디버그 빌드에서 클라이언트가 자기 Plan 해시를 산출하여 서버에 RPC로 전송, 서버가 자기 Plan과 비교하여 불일치 시 경고 로그. 1차에서는 수동 PIE 검증으로 시작.

---

## 7. 향후 작업 (Out of Scope — 명시적으로 미룸)

### 7.1 JIP (Join-In-Progress) 대응 — **2차 작업으로 연기**

- **현재 결정**: 1인 개발 + 모바일 러너 특성상 JIP 시나리오가 드물어 1차 구현에서는 제외.
- **대신 1차에서 보장**: 세션 시작 시점(BeginPlay 직후 + `OnRep_SharedTrackSeed` 직후)에는 서버·모든 클라이언트가 동일한 시드와 동일한 Plan을 갖도록 보장.
- **2차 작업으로 미루는 항목**:
  - `CleanupWatermark` ~ `CurrentSegmentIndex` 범위 catch-up 로직 (이미 GameState에 변수는 선언됨, 실제 사용은 미구현)
  - 신규 클라이언트 접속 시 빠른 재생성 점프
  - PathSegments의 압축 스냅샷 또는 Replay 방식 결정
- **준비 사항**: 본 v2.0 설계가 `FExSpawnPlan` 구조를 도입하므로, 향후 JIP 클라이언트가 자기 시드로 과거 청크들의 Plan을 재산출하여 Catch-up하는 기반이 자연스럽게 마련됨.

### 7.2 결정론 자동 검증 시스템

- 디버그 빌드에서 클라가 자기 Plan 해시를 산출 → 서버에 RPC로 보내 비교 → 불일치 시 경고 로그.
- 1차에서는 수동 PIE 테스트로 검증, 자동화는 2차로.

### 7.3 청크별 재시드 (Stateless RNG)

- 현재 단일 스트림. 조건부 분기로 인한 누적 드리프트가 발견되면 청크 인덱스 기반 재시드로 격상.
- 1차는 단일 스트림, 문제 발견 시 2차로 도입.

---

## 8. 참고 클래스 매핑

| 클래스 | 변경 범위 |
|---|---|
| `AExRunnerGameState` | **소폭 변경** — `OnRep_SharedTrackSeed`에 매니저 `InitializeRandomStream` 호출 추가 (§3.1.1 초기화 계약). 멤버 변수는 변경 없음 |
| `AExRunnerGameMode` | **소폭 변경** — `StartRunnerGame`에 매니저 `InitializeRandomStream` 호출 추가 (서버 측 §3.1.1 초기화 계약) |
| `UExPathManager` | 변경 없음 (이미 결정론 적용됨) |
| `UExChunkSpawner` | `SpawnNextChunk` 마지막 호출부를 Generate(양측) / Realize(서버) 2단 분기로 변경 (§3.5). ItemPlan에 ObstaclePlan을 직접 전달하도록 시그니처 변경. 청크 활성화 시 `Chunk->SegmentIndex` 기록 |
| `UExObstacleManager` | **대폭 개편** — Plan/Realize 분리, `ObstacleRandomStream` + `InitializeRandomStream` 신규, `QueryObstaclePlanAtDistance` 신규, `SpawnedBySegment` 인덱스 + `PendingAttachQueue` 신규, `OnChunkDespawned` 인덱스 기반 회수로 재작성 |
| `UExRunnerItemManager` | **대폭 개편** — 동일 패턴, `ItemRandomStream`, 뱀 패턴 상태값 결정론 보장(호출 순서 어설트 포함), ObstaclePlan 파라미터 입력, `SpawnedBySegment` 인덱스 + `PendingAttachQueue` 신규 |
| `UExItemSpawnManagerBase` | `SpawnItem` 진입부에 `if (!HasAuthority()) { ensureMsgfAlways; return; }` 이중 가드 추가 (§3.4). `AttachToActor` 호출 제거 |
| `AExItemPickupBase` | **신규 Replicated 멤버** — `int32 OwnerSegmentIndex` + `OnRep_OwnerChunkKey` 콜백 (§3.3 Manual Re-Attach). `bAlwaysRelevant = false` 변경 검토, `NetUpdateFrequency` 하향 검토 (Phase 5) |
| 장애물 액터 클래스 | `AExItemPickupBase`와 동일한 Replicated `OwnerSegmentIndex` + OnRep 패턴 적용 |
| `AExFloorChunk` | **소폭 변경** — `int32 SegmentIndex` 멤버 추가 (PathManager의 세그먼트 인덱스 보관용). 기존 KillZ/회수 로직 변경 없음 |
| `FExSpawnPlan` | **신규** (`ExCore/Struct/Items/`). `PathDistance` 오름차순 정렬 불변식 유지 |

---

## 9. 기대 효과

- **클라이언트에서 장애물·아이템 정상 출현** (현 버그 해소)
- **모든 스폰 시스템이 "Seed → Plan → Realize" 통일 패턴**으로 정렬되어 가독성·유지보수성 극대화
- **물리·상호작용 권위 단일화**로 파쿠르·점프·픽업 desync 위험 원천 차단
- **JIP 대응 인프라**가 자연스럽게 마련됨 (Plan 산출이 결정론적으로 가능)
- **데디케이티드 ↔ 리슨 서버 자유 전환** (Authority 가드만으로 동작 분기)
- **네트워크 비용 최소화**: 청크 데이터 자체는 동기화 안 함, 액터만 1회 초기 Replicate

---

## 10. 승인 요청

본 v2.2 설계는 다음을 핵심으로 한다:

1. **C안 채택** — 레이아웃 결정론(양측) + 액터 권위(서버), 판정 안전성 우선
2. **`FExSpawnPlan`** 신규 구조체로 모든 스폰을 통일된 3단 구조(Seed → Plan → Realize)로 정렬. **PathDistance 오름차순 정렬 불변식** 유지 (v2.2)
3. **시스템별 독립 RandomStream** — `PathRandomStream`(기존) + `ObstacleRandomStream` + `ItemRandomStream`
4. **§3.1.1 초기화 순서 계약** — 시드 확정 → 매니저 Stream Initialize → Generate 허용
5. **§3.3 Manual Re-Attach 모드** — UE 엔진 표준 Attach Replication이 Non-Replicated 부모를 지원하지 않음을 확인 후, **서버 World Space 스폰 + Replicated SegmentIndex + 클라 OnRep 수동 Attach** 모델로 전환 (v2.2 핵심 변경)
6. **Plan 기반 Query 규칙** — `GenerateItemPlan`은 `GenerateObstaclePlan` 결과를 직접 입력으로 받음, `QueryObstaclePlanAtDistance` 신규
7. **`if + ensure` 이중 Authority 가드 표준** — 차단 + 알림 역할 분리
8. **누적 상태값 드리프트 명시** — 1차는 호출 순서 어설트 의존, 2차에 Stateless 전환 (v2.2)
9. **JIP는 2차 작업으로 명시적 연기**

### v2.2 외부 검토 반영 결과

| 검토 항목 | 판정 | 반영 위치 |
|---|---|---|
| 1. Replicated→Non-Replicated Attach 실패 (치명) | **완전 타당, 즉시 반영** | §3.3 모드 A 폐기, Manual Re-Attach 모드 채택. Phase 4 재작성. 매핑표 변경 |
| 2. 누적 상태값 드리프트 위험 | **타당, 반영** | §3.4 누적 상태값 드리프트 절 신설. 호출 순서 어설트 추가. 2차 Stateless 전환 경로 명시 |
| 3. 이진 탐색 성능 팁 | **타당하나 1차 부담 낮음, 부분 반영** | §3.4 Plan 정렬 불변식 신설 — 정렬은 보장(이진 탐색 즉시 전환 가능), 1차는 선형 탐색 유지 |

### v2.1 외부 검토 반영 결과 (이전 갱신, 참조용)

| 검토 항목 | 판정 | 반영 위치 |
|---|---|---|
| 1. 초기화 순서 불명확 | 타당, 반영 | §3.1.1 초기화 계약 신설 |
| 2. Attach 정책 상충 | 타당, 반영 | §3.3 모드 분리 (v2.2에서 모드 A 폐기로 재정리) |
| 3. Item Plan 결정론 입력원 | 타당, 반영 | §3.4 Plan 기반 Query |
| 4. Authority 가드 약함 | 부분 반영 | `if + ensure` 이중 패턴 |
| 5. 네트워크 KPI | 부분 반영 | Phase 5 경량 튜닝 |
| 6. 매핑표 일관성 | 타당, 반영 | §8 GameState/GameMode 변경 명시 |

---

승인 후 Phase 1부터 순차 구현을 시작하겠습니다.
