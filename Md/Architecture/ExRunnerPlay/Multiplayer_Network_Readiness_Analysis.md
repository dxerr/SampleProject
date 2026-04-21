# ExRunnerPlay 멀티플레이어 네트워크 대응 분석 보고서

> **작성일**: 2026-04-21  
> **최종 수정**: 2026-04-21 (리뷰 피드백 7건 반영, Rev.2)  
> **분석 대상**: `Multiplayer_Runner_Architecture.md` 계획서 vs 현재 코드베이스  
> **목적**: 데디케이티드 서버 / 리슨 서버 환경에서 정상 동작하기 위한 현재 구조의 문제점 식별 및 해결 방안 도출

---

## 1. 분석 요약 (Executive Summary)

현재 ExRunnerPlay는 **싱글 플레이어 전용 구조**로 설계되어 있다. 가이드라인 §1.8에서 "서버 권한(Authority) 고려"를 명시하고 있으나, 실제 구현은 `GameMode`에 핵심 로직이 집중되어 있어 **클라이언트에서 트랙이 보이지 않고, 다중 플레이어의 거리 추적이 불가능**하다.

계획서에서 제시한 "결정론적 스폰" 전략은 올바른 방향이나, 현재 코드에서 이를 적용하려면 **7개 카테고리 총 18건의 구조적 문제**를 해결해야 한다.

---

## 2. 스폰 도메인 매트릭스 (Spawn Domain Matrix)

멀티플레이어 전환 시 가장 혼란이 생기기 쉬운 것이 **"이 액터는 누가 스폰하고, 리플리케이트하는가?"**이다. 모든 구현의 기준이 되는 매트릭스를 아래에 고정한다.

| 액터 유형 | bReplicates | 스폰 주체 | 함수 분리 | 비고 |
|-----------|-------------|-----------|-----------|------|
| AExFloorChunk (바닥) | `false` | **로컬** (서버+클라 각자) | `SpawnLocalChunk()` — Authority 가드 없음 | Seed 기반 결정론적 |
| 고정 장애물 (Static Obstacle) | `false` | **로컬** (Chunk와 함께) | `SpawnLocalObstacle()` — Authority 가드 없음 | Seed 기반 결정론적 |
| 동적/상호작용 장애물 | `true` | **서버 전용** | `SpawnAuthorityObstacle()` — HasAuthority 필수 | 서버에서 리플리케이트 |
| 아이템 (코인/버프) | `true` | **서버 전용** | `SpawnAuthorityItem()` — HasAuthority 필수 | World Space 절대좌표 |

> **핵심 원칙**: 로컬 스폰 함수에는 `HasAuthority()` 가드를 **넣지 않는다**. Authority 스폰 함수에는 **반드시 넣는다**. 두 역할을 하나의 함수에 섞지 않는다.

---

## 3. 카테고리별 상세 분석

### 3.1 [CRITICAL] GameMode 종속 로직 — 클라이언트 동작 불가

**현상**: `AExRunnerGameMode`는 **서버에서만 존재**하는 액터이다. 현재 `ChunkSpawner`, `ObstacleManager`, `ItemManager`, `BeatSyncComponent`, `RuleManagerComponent`가 모두 GameMode의 `CreateDefaultSubobject`로 생성되어 있어, 클라이언트에서는 이 컴포넌트들이 **아예 존재하지 않는다**.

| 문제 ID | 파일 | 문제 | 심각도 |
|---------|------|------|--------|
| GM-01 | `ExRunnerGameMode.h:104-116` | `ChunkSpawner`, `ObstacleManager`, `ItemManager`가 GameMode에 생성됨 → 클라이언트에서 트랙/장애물/아이템 없음 | CRITICAL |
| GM-02 | `ExRunnerGameMode.cpp:161-177` | `Tick()`에서 `Player 0`만 거리 추적 (`GetPlayerPawn(0)`) → 멀티플레이어 거리 동기화 불가 | CRITICAL |
| GM-03 | `ExRunnerGameMode.cpp:87-139` | `StartRunnerGame()`이 서버에서만 실행 → 클라이언트 스포너 초기화 불가 | CRITICAL |

**계획서 대응**: §2.1 결정론적 트랙 생성, §2.2 다중 플레이어 거리 동기화

**해결 방향**:
- `ChunkSpawner`, `ObstacleManager`를 `AExRunnerGameState`로 이관 (클라이언트에서도 접근 가능)
- 또는 `UExRunnerWorldSubsystem`을 도입하여 서버/클라이언트 모두에서 로컬 인스턴스를 가동
- 거리 추적은 §3.7에서 정의하는 **PlayerState 계층 분리 모델**을 적용

---

### 3.2 [CRITICAL] 트랙 스폰 권한 제어 — 스폰 도메인 혼재

**현상**: `ExChunkSpawner::SpawnNextChunk()`에 이미 `HasAuthority()` 체크가 존재한다 (L119-123). 이는 서버에서만 스폰하겠다는 의도이나, **결정론적 로컬 스폰 방식으로 전환하면 이 가드가 역으로 클라이언트의 로컬 스폰을 차단**하는 문제가 된다. 반면 동적 아이템은 서버 전용 스폰이 필요하므로, **하나의 가드 정책으로 통일할 수 없다**.

| 문제 ID | 파일 | 문제 | 심각도 |
|---------|------|------|--------|
| CS-01 | `ExChunkSpawner.cpp:119-123` | `SpawnNextChunk`에 `HasAuthority()` 가드 → 결정론적 로컬 스폰 시 클라이언트 차단 | CRITICAL |
| CS-02 | `ExChunkSpawner.cpp:270-278` | `ReturnChunkToPool`에 `HasAuthority()` 가드 → 클라이언트 풀 반환 불가 | CRITICAL |
| CS-03 | `ExChunkSpawner.cpp:119-120` | Owner가 GameMode → 클라이언트에서 Owner가 null → 가드 조건 `GetOwner() && !GetOwner()->HasAuthority()`에서 단락 평가로 크래시는 발생하지 않으나, Owner null 시 가드를 통과하여 의도치 않은 스폰이 실행될 수 있음 (no-op 또는 오스폰) | MEDIUM |

**해결 방향**:
- §2 스폰 도메인 매트릭스 기준으로 함수를 물리적으로 분리
- 로컬 스폰 계열(`SpawnLocalChunk`, `SpawnLocalObstacle`): Authority 가드 제거
- 서버 스폰 계열(`SpawnAuthorityItem`, `SpawnAuthorityObstacle`): Authority 가드 필수

---

### 3.3 [CRITICAL] Join-in-Progress 복구 시나리오 미계획

**현상**: 계획서와 이전 분석에서 `SharedTrackSeed` 복제만 언급했으나, 이것만으로는 **게임 진행 중 접속한 신규 클라이언트**가 현재 활성 세그먼트 상태를 정확히 재구성할 수 없다.

`PathManager`의 `PathSegments`는 `UPROPERTY()`이지만 **Replicated가 아니다** (ExPathManager.h:137). `SetIsReplicated(true)`는 컴포넌트 자체의 존재를 리플리케이트할 뿐, 내부 `TArray<FExPathSegment>` 데이터를 복제하지 않는다. 따라서 중간 접속 클라이언트는 Seed를 받아도 **"몇 번째 세그먼트부터 생성을 시작해야 하는지"를 알 수 없다**.

Seed만으로 처음부터 N번 돌려 따라잡는 방법(Catch-up Replay)도 이론적으로 가능하나, 세그먼트 수가 많아지면 초기화 비용이 급증하며, 결정론적 소비 순서가 1회라도 어긋나면 이후 전체가 불일치한다.

| 문제 ID | 파일 | 문제 | 심각도 |
|---------|------|------|--------|
| JIP-01 | `ExRunnerGameState.h` | 중도 접속 복구용 스냅샷 데이터 미선언 | CRITICAL |
| JIP-02 | `ExPathManager.h:137` | `PathSegments`가 Replicated 아님 → 신규 클라이언트가 경로를 모름 | CRITICAL |

**해결 방향**:
`AExRunnerGameState`에 다음 스냅샷 동기화 항목을 Replicated로 추가한다:

| 변수 | 타입 | 용도 |
|------|------|------|
| `SharedTrackSeed` | `int32` | 결정론적 경로 생성의 공통 시드 |
| `CurrentSegmentIndex` | `int32` | 현재까지 생성된 세그먼트 인덱스 (신규 클라 catch-up 범위 결정) |
| `SegmentStartDistance` | `float` | 현재 선두 세그먼트의 시작 누적 거리 |
| `CleanupWatermark` | `float` | 이 거리 이전의 세그먼트는 이미 정리됨 (신규 클라가 생성 생략할 범위) |

신규 클라이언트는 접속 시 `CleanupWatermark ~ CurrentSegmentIndex` 범위만 Seed 기반으로 빠르게 재생성하고, 이전 구간은 건너뛴다.

---

### 3.4 [CRITICAL] 결정론적 설계의 스트림 드리프트 위험

**현상**: 계획서와 이전 분석에서 "단일 `FRandomStream` 주입"을 제안했으나, 현재 랜덤 호출이 시스템별·조건별로 분산되어 있어 **소비 횟수가 쉽게 어긋나는 구조**이다.

실제 코드에서 확인된 비결정론적 랜덤 호출 지점:

| 위치 | 호출 | 조건부 여부 |
|------|------|------------|
| `ExPathManager.cpp:189` | `FMath::FRand()` — 커브 확률 판정 | 항상 호출 |
| `ExPathManager.cpp:261` | `FMath::RandBool()` — 좌/우 방향 결정 | **커브일 때만** (직선이면 스킵) |
| `ExObstacleManager.cpp:163` | `FMath::FRand()` — 스폰 확률 판정 | 항상 호출 |
| `ExObstacleManager.cpp:265` | `FMath::RandRange()` — 정의 선택 | **스폰 결정 시에만** |
| `ExRunnerItemManager.cpp:126+` | `FMath::FRand()` × 다수 | 조건부 다수 (코인/버프/뱀패턴) |

`FMath::RandBool()`은 직선 판정 시 호출되지 않고, 장애물 스폰 확률 실패 시 `SelectRandomDefinition`은 호출되지 않는다. 이런 **조건부 분기 때문에 서버-클라이언트 간 랜덤 소비 횟수가 1회라도 어긋나면 이후 모든 경로·장애물·아이템이 불일치**한다.

| 문제 ID | 파일 | 문제 | 심각도 |
|---------|------|------|--------|
| RNG-01 | `ExPathManager.cpp:189,261` | 조건부 분기로 스트림 소비 횟수 불일치 가능 | CRITICAL |
| RNG-02 | `ExObstacleManager.cpp:163,265` | 스폰 확률 실패 시 소비 횟수 분기 | CRITICAL |
| RNG-03 | `ExRunnerItemManager.cpp:126+` | 다수의 조건부 랜덤 호출이 분산 | CRITICAL |

**해결 방향 — 시스템별 독립 `FRandomStream` 분리**:

단일 스트림 공유 대신, 각 시스템에 **독립적인 `FRandomStream`** 을 할당한다. UE5의 `FRandomStream`은 이 용도에 맞게 설계되어 있으며, 각 시스템이 자기 스트림만 소비하므로 한 시스템의 조건부 분기가 다른 시스템에 영향을 주지 않는다.

| 스트림 | Seed 생성 규칙 | 사용처 |
|--------|---------------|--------|
| `PathStream` | `Hash(SharedTrackSeed, 0)` | PathManager — 커브 확률, 방향 결정 |
| `ObstacleStream` | `Hash(SharedTrackSeed, 1)` | ObstacleManager — 스폰 확률, 정의 선택 |
| `ItemStream` | `Hash(SharedTrackSeed, 2)` | ItemManager — 코인/버프/뱀 패턴 |

추가 안전 장치로, 각 스트림 내부에서도 **세그먼트 인덱스 기반 재시드(Stateless RNG)** 를 선택적으로 적용할 수 있다: `Stream.Initialize(Hash(BaseSeed, SegmentIndex))`. 이렇게 하면 한 세그먼트에서 소비 횟수가 어긋나더라도 다음 세그먼트에서 자동 복구된다.

---

### 3.5 [HIGH] FloorChunk KillZ 판정 — CachedGameMode 게이트 문제

**현상**: `AExFloorChunk`의 KillZ 판정 코드에서, 실제 거리값은 이미 **GameState에서 읽고 있다** (`GS->CurrentPathDistance`). GameMode는 데이터 소스가 아니다. 핵심 문제는 **`CachedGameMode`가 존재할 때만 KillZ 계산 블록에 진입하는 게이트 조건**이다.

```cpp
// ExFloorChunk.cpp:82-91 (현재 코드)
if (CachedGameMode)                    // ← 이것이 문제의 본질 (게이트)
{
    float PlayerDist = 0.f;
    if (AExRunnerGameState* GS = GetWorld()->GetGameState<AExRunnerGameState>())
    {
        PlayerDist = GS->CurrentPathDistance;   // ← 실제 데이터는 GameState에서 읽음
    }
    bReachedKillZ = (PathDistance < PlayerDist + KillZ);
}
```

클라이언트에서는 `CachedGameMode`가 null이므로 이 블록 전체가 스킵되어, 청크가 영원히 삭제되지 않는다.

| 문제 ID | 파일 | 문제 | 심각도 |
|---------|------|------|--------|
| FC-01 | `ExFloorChunk.cpp:82` | `if (CachedGameMode)` 게이트 → 클라이언트에서 KillZ 블록 진입 불가 | HIGH |
| FC-02 | `ExFloorChunk.cpp:56` | `CachedGameMode` 캐싱 자체가 네트워크 비대칭의 원인 | HIGH |
| FC-03 | `ExFloorChunk.h:18,187` | `AExRunnerGameMode` 전방 선언 및 멤버 변수 — 불필요한 의존성 | MEDIUM |

**해결 방향**:
- `CachedGameMode` 게이트를 제거하고, **GameState 유효성 기준으로 판정**하도록 변경
- `CachedGameMode` 멤버 변수 및 전방 선언 자체를 삭제하여 의존성 제거
- 또는 KillZ 판정 자체를 SpawnerComponent 내부로 이관하여 FloorChunk의 독립 Tick을 제거

```cpp
// 수정 방향 (게이트를 GameState 기준으로 교체)
if (AExRunnerGameState* GS = GetWorld()->GetGameState<AExRunnerGameState>())
{
    float PlayerDist = GS->CurrentPathDistance;
    bReachedKillZ = (PathDistance < PlayerDist + KillZ);
}
```

---

### 3.6 [HIGH] 아이템 리플리케이션 — Attach 좌표 깨짐 및 C++ 런타임 가드 부재

**현상 1 — Attach 좌표 깨짐**: `ExRunnerItemManager.cpp:291`에서 스폰된 아이템을 `AttachToActor(Chunk, KeepWorldTransform)`으로 바닥에 부착하고 있다. 바닥(FloorChunk)이 로컬 전용(`bReplicates=false`)이므로, 아이템이 `bReplicates=true`가 되면 클라이언트에서 Chunk의 NetGUID가 존재하지 않아 **Attach 해소 후 위치가 깨진다**.

**현상 2 — C++ 런타임 가드 부재**: `ExItemSpawnManagerBase::SpawnItem()` (L35)에 `HasAuthority()` 체크가 전혀 없다. `BlueprintAuthorityOnly`는 BP 그래프 호출만 제한하며, C++ 코드에서 `SpawnItem()`을 직접 호출하면 클라이언트에서도 실행된다. ChunkSpawner가 GameState로 이관되어 클라이언트에서도 동작하게 되면 **클라이언트에서 아이템 오스폰이 발생**할 수 있다.

| 문제 ID | 파일 | 문제 | 심각도 |
|---------|------|------|--------|
| IT-01 | `ExRunnerItemManager.cpp:291` | `AttachToActor(Chunk, KeepWorldTransform)` → 리플리케이트 아이템이 로컬 Chunk에 Attach되어 위치 깨짐 | HIGH |
| IT-02 | `ExItemSpawnManagerBase.cpp:35` | `SpawnItem()`에 C++ HasAuthority 가드 없음 → 클라이언트 오스폰 위험 | HIGH |

**해결 방향**:
- 아이템은 **World Space(절대 좌표)** 스폰, FloorChunk에 Attach 금지
- `SpawnItem()` 시작부에 명시적 Authority 가드 추가:
  ```cpp
  if (!ensure(GetOwner() && GetOwner()->HasAuthority()))
  {
      UE_LOG(LogExItemSystem, Warning, TEXT("SpawnItem: 서버 권한이 아닌 환경에서 호출됨. 스폰 생략."));
      return nullptr;
  }
  ```

---

### 3.7 [HIGH] 멀티플레이어 거리 동기화 — PlayerState 계층 분리

**현상**: 이전 분석에서 "GameState::Tick에서 모든 활성 폰을 순회하여 Lead/Tail 산출"만 제안했으나, 각 플레이어 진행 거리의 **권위적 저장소(Authoritative Source of Truth)**가 불명확하다. 현재는 `Player 0` 단일 추적(`GetPlayerPawn(0)`)이다.

| 문제 ID | 파일 | 문제 | 심각도 |
|---------|------|------|--------|
| DS-01 | `ExRunnerGameMode.cpp:215` | `GetPlayerPawn(0)` → 호스트만 추적, 다른 플레이어 무시 | HIGH |
| DS-02 | `ExRunnerGameState.h` | Lead/Tail 계산의 입력 데이터(개별 플레이어 거리)가 정의 안됨 | HIGH |

**해결 방향 — PlayerState 기반 계층 분리**:

현재 `AExPlayerStateBase`가 이미 존재하며 `Score`를 Replicated로 관리하고 있다. 동일한 패턴으로 `ServerAuthPathDistance`를 추가하여 계층을 분리한다:

| 계층 | 클래스 | 변수 | 역할 |
|------|--------|------|------|
| **개별 플레이어** | `AExRunnerPlayerState` (신설 또는 기존 확장) | `ServerAuthPathDistance` (Replicated) | 서버 권한으로 각 플레이어의 진행 거리를 기록 |
| **글로벌 통계** | `AExRunnerGameState` | `LeadDistance`, `TailDistance` (Replicated) | PlayerState 배열에서 파생한 통계만 복제 |

GameState::Tick에서는 PlayerState 목록을 순회하여 Max/Min을 산출하고, ChunkSpawner는 `LeadDistance`로 앞 트랙을 깔고 `TailDistance`로 뒤 트랙을 삭제한다.

---

### 3.8 [MEDIUM] Seed 동기화 변수 미구현

**현상**: 현재 코드에 `RandomStream` 또는 Seed 기반 생성 로직이 전혀 없다.

| 문제 ID | 파일 | 문제 | 심각도 |
|---------|------|------|--------|
| SD-01 | `ExRunnerGameState.h` | `SharedTrackSeed` Replicated 변수 미선언 | MEDIUM |

**해결 방향**: §3.3의 스냅샷 동기화 항목에 포함하여 `SharedTrackSeed`를 GameState에 추가한다. Seed 생성은 `GameMode::BeginPlay`에서 `FMath::Rand()` → GameState에 설정한다.

---

## 4. 현재 구조에서 이미 멀티플레이 대응이 된 부분

분석 결과 아래 항목들은 **이미 올바르게 구현**되어 있거나 계획서의 의도에 부합한다:

| 항목 | 상태 | 설명 |
|------|------|------|
| `AExRunnerGameState` 리플리케이션 | ✅ 정상 | `CurrentPathDistance`, `RealPlayerPathDistance`, `RemainingTimeSeconds`, `GameOverReason` 모두 Replicated |
| `UExPathManager`가 GameState에 부착 | ✅ 정상 | 이미 `GameMode`에서 `GameState`로 이관 완료, `SetIsReplicated(true)` |
| 매치 페이즈 흐름 | ✅ 정상 | `ExGameStateBase::CurrentMatchPhase` Replicated, `OnRep_MatchPhase` 구현 |
| 룰 시스템 서버 권한 | ✅ 정상 | `OnRuleEndGameEvent`, `OnRuleTriggered` 모두 `HasAuthority()` 체크 |
| `ExRunnerMovementComponent` | ✅ 부분 | Mover 2.0 기반 클라이언트 예측 프레임워크 사용 중 |
| `AExFloorChunk::bReplicates` | ✅ 정상 | 기본값 `false` — 계획서와 일치 |
| `ExGameModeBase::bUseSeamlessTravel` | ✅ 정상 | Seamless Travel 활성화 |
| `AExPlayerStateBase` 기반 구조 | ✅ 정상 | Score Replicated, OnRep 패턴 — 확장 가능 |

---

## 5. 마이그레이션 우선순위 (Phase 8 Checklist 대응)

계획서의 Phase 8 Checklist와 대조하여, 리뷰 피드백을 반영한 구현 우선순위를 재정리한다:

### Phase 8-A: 기반 구조 이관 (CRITICAL — 먼저 수행)

1. **GameState에 스냅샷 동기화 변수 추가** (SD-01, JIP-01)
   - `int32 SharedTrackSeed` (Replicated)
   - `int32 CurrentSegmentIndex` (Replicated) — Join-in-Progress 대응
   - `float SegmentStartDistance` (Replicated)
   - `float CleanupWatermark` (Replicated) — 정리 완료 기준점
   - `float LeadDistance`, `float TailDistance` (Replicated)
   
2. **ChunkSpawner / ObstacleManager 이관 + 함수 분리** (GM-01, GM-03, CS-01, CS-02)
   - `GameMode`에서 `GameState`로 로컬 스폰 계열 컴포넌트 이동
   - §2 스폰 도메인 매트릭스 기준으로 `SpawnLocal*` / `SpawnAuthority*` 함수 물리 분리
   - 기존 `HasAuthority()` 가드를 도메인별로 재배치

3. **PlayerState 계층 분리** (DS-01, DS-02, GM-02)
   - `AExRunnerPlayerState`에 `ServerAuthPathDistance` (Replicated) 추가
   - `GameState::Tick`에서 PlayerState 순회 → Lead/Tail 산출

### Phase 8-B: 결정론적 스폰 인프라 (CRITICAL)

4. **시스템별 독립 FRandomStream 도입** (RNG-01, RNG-02, RNG-03)
   - `PathStream`, `ObstacleStream`, `ItemStream` 분리
   - 모든 `FMath::FRand()`, `FMath::RandBool()`, `FMath::RandRange()` → `Stream.FRand()` 등으로 교체
   - 선택적으로 세그먼트 인덱스 기반 재시드(Stateless RNG) 적용

5. **FloorChunk의 CachedGameMode 게이트 제거** (FC-01, FC-02, FC-03)
   - KillZ 판정을 `GameState` 유효성 기준으로 전환
   - `CachedGameMode` 멤버 및 전방 선언 삭제

6. **Join-in-Progress 복구 로직** (JIP-02)
   - 신규 클라이언트 접속 시 `CleanupWatermark ~ CurrentSegmentIndex` 범위만 Seed 기반 빠른 재생성
   - 이전 구간 건너뛰기

### Phase 8-C: 리플리케이션 분리 (HIGH)

7. **아이템 World Space 스폰 + Authority 가드** (IT-01, IT-02)
   - `AttachToActor(Chunk)` → 절대 좌표 방식으로 전환
   - `SpawnItem()` 시작부에 `ensure(HasAuthority())` 추가
   - `bReplicates = true` 명시 설정

---

## 6. 아키텍처 선택지: GameState 이관 vs WorldSubsystem 신설

계획서에서 두 가지 선택지를 제시했다. 각각의 장단점을 정리한다:

### Option A: `AExRunnerGameState`에 컴포넌트 이관

**장점**:
- 이미 `PathManager`가 GameState에 있으므로 패턴 일관성 유지
- Replicated 변수와 같은 레벨에 존재하여 데이터 접근이 자연스러움
- `GetWorld()->GetGameState<>()` 로 서버/클라이언트 모두 접근 가능

**단점**:
- GameState가 비대해짐 (PathManager + ChunkSpawner + ObstacleManager + ItemManager + BeatSync + RuleManager)
- 리플리케이션 대역폭 관리 주의 필요 (컴포넌트 자체가 리플리케이트될 수 있음)
- `AGameStateBase`는 `AInfo` 계열이라 Tick 비용이 낮지만, 컴포넌트가 많아지면 관리 복잡도 증가

### Option B: `UExRunnerWorldSubsystem` 신설

**장점**:
- `UWorldSubsystem`은 서버/클라이언트 모두에서 독립적으로 인스턴스화
- GameState 비대화 방지
- 순수 로컬 로직 (스폰, 풀링)과 네트워크 동기화 로직을 깔끔하게 분리 가능

**단점**:
- Subsystem은 `UPROPERTY(Replicated)` 지원 안됨 → Seed 동기화는 별도 채널(GameState) 필요
- 기존 패턴(PathManager가 GameState 컴포넌트)과 불일치 → 두 곳에 나뉘어 복잡도 증가
- Subsystem의 초기화 타이밍이 GameState보다 이를 수 있어 의존성 관리 필요

### 권장: **Option A (GameState 이관)** + 역할 분리

- Seed 동기화, 거리 추적 등 **Replicated 데이터**: GameState 직접 관리
- ChunkSpawner, ObstacleManager 등 **로컬 실행 엔진**: GameState의 컴포넌트로 이관하되, 서버/클라이언트 분기 로직은 컴포넌트 내부에서 처리
- RuleManager, BeatSync 등 **서버 전용 로직**: GameMode에 잔류

---

## 7. 리슨 서버 특수 고려사항

리슨 서버에서는 호스트가 서버이자 클라이언트이므로 다음 사항에 주의해야 한다:

| 문제 | 설명 | 해결 |
|------|------|------|
| 바닥(Floor) 이중 스폰 | 호스트에서 서버 로직 + 클라이언트 로직 양쪽이 스폰 시도 | GameState 컴포넌트의 `BeginPlay`에서 한 번만 초기화. GameState는 월드당 1개이므로 자체 이중 스폰은 발생하지 않음 |
| 아이템/동적 장애물 더블 스폰 | 스포너가 로컬로 이관되면, 호스트에서 로컬 스폰 로직이 서버 리플리케이트와 겹칠 수 있음 | §2 스폰 도메인 매트릭스 준수: 아이템/동적 장애물은 **오직 `HasAuthority()` 분기 내에서만** 스폰하고, 클라이언트는 리플리케이션으로만 수신 |
| KillZ 이중 판정 | 서버 Tick + 클라이언트 Tick 양쪽에서 KillZ 발동 | KillZ 판정을 SpawnerComponent 내부에서만 수행 (FloorChunk의 독립 Tick 제거) |
| `GetPlayerPawn(0)` 충돌 | 리슨 서버에서 Index 0은 호스트 → 다른 플레이어 무시 | §3.7 PlayerState 계층 분리 모델로 대체 |
| 데디케이티드 서버 시각 최적화 | 데디케이티드 서버에는 렌더링 없음 → SplineMesh 등 시각 요소 불필요 | 로컬 스폰 로직에 `IsRunningDedicatedServer()` 분기 삽입, 시각 컴포넌트 스킵 |

---

## 8. 데디케이티드 ↔ 리슨 서버 자유 전환 보장

본 보고서의 설계는 **서버 유형에 의존하지 않는 역할 분리**를 기본 원칙으로 한다. 이를 통해 프로젝트 설정이나 커맨드라인에서 데디케이티드 ↔ 리슨을 자유롭게 전환해도 코드 수정 없이 정상 동작한다.

| 동작 | 데디케이티드 서버 | 리슨 서버 |
|------|---|---|
| Seed 생성 | 서버(렌더링 없음) | 호스트(렌더링 있음) |
| 트랙 로컬 스폰 | 클라이언트만 (서버는 `IsRunningDedicatedServer()` 시 시각 스킵) | 호스트 + 클라이언트 모두 |
| 아이템 리플리케이션 | 서버 → 클라이언트 | 호스트(서버) → 클라이언트 |
| Authority 판정 | 서버 | 호스트 |

`HasAuthority()`는 데디케이티드든 리슨이든 **Authority 측에서만 true**를 반환하므로, §2 스폰 도메인 매트릭스에 따라 함수를 분리하면 서버 유형 변경에 투명(transparent)하다.

---

## 9. 결론 및 다음 단계

현재 코드베이스는 멀티플레이어 환경에서 **정상 동작하지 않는다**. 핵심 원인은 `GameMode`에 종속된 트랙 생성 시스템, `Player 0` 단일 추적 구조, 결정론적 보장 부재, Join-in-Progress 미계획이다.

리뷰 피드백 7건을 반영하여 분석을 **7개 카테고리 18건**으로 확장했으며, 특히 다음 3가지가 핵심 추가사항이다:

1. **스폰 도메인 매트릭스** (§2) — 로컬 vs 서버 스폰의 명확한 경계
2. **시스템별 독립 FRandomStream** (§3.4) — 드리프트 방지
3. **Join-in-Progress 스냅샷 동기화** (§3.3) — 중도 접속 복구

Phase 8-A/B/C 순서로 해결하면 데디케이티드 서버와 리슨 서버 모두에서 안정적인 멀티플레이 러너 환경을 구축할 수 있다.

**주인님의 승인 후 Phase 8-A 구현을 시작하겠습니다.**
