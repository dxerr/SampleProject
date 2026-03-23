# ExFrameWork: 아이템 시스템 아키텍처 설계서

> **버전:** v1.2  
> **대상 엔진:** Unreal Engine 5  
> **프로젝트:** ExFrameWork  
> **작성일:** 2026-03-23  
> **최종 수정:** 2026-03-23 (v1.2: 중앙 제어식 스폰 순서 보장, 곡선/점프 아치(Arc) Z축 보간, 일반 곡선 청크 기본 스폰 명시)  
> **설계 방식:** A안 (Effect-as-Object) + B안 (Tag-Driven Event) 결합  
> **의존 문서:**  
> - ExFrameWork_Multiplayer_Flow_Architecture.md (v2.0)  
> - ExFrameWork_UI_System_Architecture.md (v2.1)  
> - ExRunner_Obstacle_System_Architecture.md  
> - ExRunner_System_Architecture.md  

---

## 1. 아키텍처 개요 및 핵심 원칙

### 1.1 목표

필드에 스폰되어 플레이어가 획득(Pickup)하면 즉시 효과를 발동하는 **수집형 아이템 시스템**을 구축한다.
데디케이티드 서버 및 리슨 서버를 완벽하게 지원하며, ExFrameWork의 Core/Feature 계층 분리 원칙을 엄격히 준수한다.

### 1.2 프로젝트 레이어 구조

```
ExCore (항상 활성화)                    ExRunnerPlay (GameFeature)
─────────────────                      ──────────────────────────
UExItemDefinition (DataAsset)          DA_RunnerCoin, DA_RunnerSpeedBoost (인스턴스)
UExItemEffect (UObject, Abstract)      UExItemEffect_SpeedBoost (러너 전용)
AExItemPickupBase (Actor)              (BP 확장: BP_RunnerCoin, BP_RunnerSpeedBoost)
UExItemSpawnManagerBase (Component)    UExRunnerItemManager (청크 연동, 월드 시프트)
FExItemSpawnEntry (Struct)             UExRunnerItemSpawnTable (DataAsset)
UExItemEffect_Score (범용)             
UExItemEffect_Buff (범용 템플릿)       
ExItemTags (GameplayTag)               ExRunnerItemTags (러너 전용 태그)
```

**레이어 배치 규칙:**

| 클래스 | 배치 위치 | 이유 |
|---|---|---|
| `UExItemDefinition` | **ExCore** | 아이템 메타데이터는 게임 모드에 무관한 범용 구조 |
| `UExItemEffect` | **ExCore** | 이펙트 베이스 및 범용 이펙트(Score, Buff)는 모든 게임 모드에서 재사용 |
| `AExItemPickupBase` | **ExCore** | 픽업 액터의 서버 권한 생성, 오버랩 감지, 풀링 인터페이스는 범용 |
| `UExItemSpawnManagerBase` | **ExCore** | 풀링, 활성화/비활성화 패턴은 범용 (ExObstacleManager와 동일 패턴) |
| `UExRunnerItemManager` | **ExRunnerPlay** | 월드 시프트 보정, 커브 청크 지원 배치, 장애물 연동 Z축(점프 아치 등) 결정 |
| `UExItemEffect_SpeedBoost` | **ExRunnerPlay** | `ExRunnerMovementComponent` 직접 참조 필요 |

> **[핵심 원칙]** Core 플러그인은 절대로 Feature 모듈의 존재를 알거나 참조해서는 안 된다.  
> Feature 플러그인은 Core를 자유롭게 `#include` 하여 상속받고 사용할 수 있다.

### 1.3 7대 설계 원칙

1. **서버 권한 생성:** 아이템 액터는 반드시 서버(GameMode/서버 권한 컴포넌트)에서만 `SpawnActor`한다. 클라이언트에서 스폰하지 않는다.
2. **데이터 드리븐:** 아이템 외형, 이펙트 타입, 수치, 스폰 확률은 모두 DataAsset에서 관리한다. 코드 수정 없이 기획 데이터만으로 새 아이템을 추가할 수 있어야 한다.
3. **Effect-as-Object:** 이펙트 로직을 독립 UObject 서브클래스로 캡슐화한다. DataAsset에서 `Instanced` 인라인 편집이 가능하다.
4. **이벤트 보조:** 이펙트 발동 시 `ExGameplayEventSubsystem`으로 태그 이벤트를 브로드캐스트한다. UI/사운드/VFX가 느슨하게 구독한다.
5. **오브젝트 풀링:** 장애물 시스템(`ExObstacleManager`)과 동일한 FIFO 풀링 패턴을 적용한다.
6. **클라이언트 예측 (v1.1):** 로컬 플레이어의 오버랩 시 서버 응답을 기다리지 않고 즉시 비주얼을 숨기고 피드백을 재생한다.
7. **중앙 제어 스폰 (v1.2):** 델리게이트 Race Condition을 방지하기 위해, `UExChunkSpawner`가 장애물 스폰을 완료한 직후 명시적으로 아이템 스폰을 순차 호출(Sequential Call)하는 중앙 제어 방식을 사용한다.

### 1.4 엄격한 제약 사항

**리플리케이션 규칙:**
- `AExItemPickupBase`는 서버에서 `SpawnActor`되므로 기본적으로 클라이언트에 복제된다.
- 아이템 획득의 **최종 판정**은 서버 오버랩에서만 수행한다. 이펙트 적용(`Execute`)은 서버에서만 호출한다.
- **클라이언트 사전 예측 (v1.1):** 로컬 플레이어(Autonomous Proxy)의 오버랩 시, 서버 확인을 기다리지 않고 로컬에서 즉시 메시 숨김 + 피드백 재생을 수행한다. 서버 Multicast가 도착하면 이미 처리된 상태이므로 중복 실행 방지 플래그(`bLocallyPredicted`)로 가드한다.
- 결과값(점수, 속도 등)은 기존 리플리케이션 경로를 통해 클라이언트에 전파된다.

**UExItemEffect Stateless 원칙 (v1.1):**
- `UExItemEffect` 계열 클래스는 **절대 런타임 상태(Member Variable)를 보유하지 않는 순수 함수형(Stateless)**으로 작성한다.
- 이펙트의 `Execute()` 함수는 파라미터로 전달받는 값만 사용하고, 상태 관리(버프 지속 시간 등)는 반드시 대상 컴포넌트에 위임한다.

**Assert 및 검증 규칙 (v1.1):**
- 서버 권한 함수(`ServerPickUp`, `Execute` 등)의 진입점에서 필수 포인터는 `ensureAlwaysMsgf`로 검증한다.

**풀링 규칙:**
- 풀 반환 시 `DetachFromActor` → `SetActorHiddenInGame(true)` → `SetActorEnableCollision(false)`.
- 풀 재사용 시 `SetActorHiddenInGame(false)` → `SetActorEnableCollision(true)`.

---

## 2. 핵심 클래스 상세 (요약)

*(기본 메타데이터/액터/이펙트 클래스의 코드는 v1.1과 동일하므로 상세 로직은 원본 참조)*
- **UExItemDefinition**: `PickupActorClass`, `Instanced ItemEffect` 보유.
- **UExItemEffect**: Stateless, `Execute()` 가상함수 제공.
- **AExItemPickupBase**: Authority 기반 Overlap + 클라이언트 예측 피드백 캡슐화.
- **UExItemSpawnManagerBase**: Pooling 매니저.

---

## 3. 장애물 연동 및 곡선 청크 아이템 배치 시스템 (v1.2 신규)

### 3.1 설계 배경

러너 게임에서 아이템은 단순히 바닥에만 배치되지 않으며, 선형(Linear) 공간만을 가정할 수 없다. 커브 구간(Curved Chunk)의 뒤틀린 표면을 따라 궤적이 정해져야 하며, **장애물 타입에 따라 Z축(높이) 위치가 곡선 또는 궤적을 그리며 자동으로 결정**되어야 한다. 이를 위해 ChunkSpawner가 전체 스폰 권한과 순서를 관장하는 중앙 제어를 모델링한다.

### 3.2 장애물 타입별 아이템 Z축 및 트랜스폼 배치 규칙

| 장애물 타입 / 상황 | 아이템 Transform / Z축 위치 | 게임플레이 의도 |
|---|---|---|
| **Climb** | 장애물 **꼭대기**(Top) 위 | 올라가야 획득. 보상으로 유도 |
| **Slide** (통과만 가능) | 장애물 **하단과 바닥 사이** 빈 공간 중앙 | 슬라이드해야 획득 |
| **Slide** (+올라갈 수 있는 경우) | 하단 빈 공간 **또는** 장애물 꼭대기 | 양쪽 선택지. 확률적 분배 |
| **Gap** | 빈 공간 위 **점프 포물선(Arc) 높이 궤적 반영** | 점프 궤적을 따라 유려하게 코인이 배치됨 |
| **None** (일반 장애물 없음 구간) | **해당 청크 표면 좌표계(GetLocalTransformAtDistance)를 따라 Z와 회전 자동 결정** | 곡선형 코스에서도 바닥의 굴곡을 따라 자연스럽게 배치됨 |

### 3.3 FExObstacleContext — 장애물 컨텍스트 질의 구조체

```cpp
USTRUCT(BlueprintType)
struct EXRUNNERPLAYRUNTIME_API FExObstacleContext
{
    GENERATED_BODY()

    bool bHasObstacle = false;
    EExObstacleType ObstacleType = EExObstacleType::None;

    /** 장애물의 월드 바운드 */
    FBox ObstacleBounds = FBox(ForceInit);

    float ObstacleTopZ = 0.f;
    float ObstacleBottomZ = 0.f;

    bool bCanClimbOver = false;
    float ClimbableHeightThreshold = 200.f;
};
```

### 3.4 중앙 제어를 위한 UExRunnerItemManager 확장

`UExRunnerItemManager`는 더 이상 `OnChunkSpawned` 이벤트를 직접 듣지 않는다. 대신 `UExChunkSpawner`가 주도권을 가지고 아이템 배치를 요청한다.

```cpp
/**
 * 청크 스포너가 장애물 배치를 완료한 후 호출하는 함수.
 * 내부적으로 Chunk의 GetLocalTransformAtDistance()를 사용해 초기 베이스 트랜스폼을 잡는다.
 */
UFUNCTION(BlueprintCallable, Category = "Runner|Item")
void SpawnItemsOnChunk(AExFloorChunk* TargetChunk, UExObstacleManager* ObstacleManager);

/**
 * 장애물 컨텍스트와 구간 내 비율을 기반으로 아이템의 최종 Z 좌표를 결정한다.
 * @param Context 장애물 질의 결과
 * @param ChunkBaseLocalZ 해당 Distance에서 청크의 바닥 기준 Z (곡선 청크 대응)
 * @param AlphaInGap Gap 장애물 통과 시 점프 궤적 커브를 평가하기 위한 상대적 거리 비율 (0~1)
 */
float CalculateItemZ(const FExObstacleContext& Context, float ChunkBaseLocalZ, float AlphaInGap) const;
```

**CalculateItemZ 핵심 구현 (포물선 및 청크 표면 반영):**
```cpp
float UExRunnerItemManager::CalculateItemZ(const FExObstacleContext& Context, float ChunkBaseLocalZ, float AlphaInGap) const
{
    if (!Context.bHasObstacle) 
    {
        // 장애물 없음 → 곡선 청크의 표면 높이를 그대로 유지
        return ChunkBaseLocalZ; 
    }

    switch (Context.ObstacleType)
    {
    case EExObstacleType::Climb:
        return Context.ObstacleTopZ + ItemTopPlacementOffset;

    case EExObstacleType::Slide:
        if (Context.bCanClimbOver && FMath::FRand() < SlideTopPlacementRatio)
        {
            return Context.ObstacleTopZ + ItemTopPlacementOffset;
        }
        return (ChunkBaseLocalZ + Context.ObstacleBottomZ) * 0.5f; // 바닥과 하단 사이 중앙

    case EExObstacleType::Gap:
        // 점프 포물선 커브 평가. Curve가 지정되어 있다면 곡선 궤적 높이를 추가 산출
        if (JumpArcCurve)
        {
            return ChunkBaseLocalZ + (JumpArcCurve->GetFloatValue(AlphaInGap) * JumpApexHeight);
        }
        return ChunkBaseLocalZ + JumpApexHeight; // 커브 미할당시 고정 높이

    default:
        return ChunkBaseLocalZ;
    }
}
```

### 3.5 중앙 제어 스폰 배치 흐름도 (Sequence)

```
[AExRunnerGameMode 시작]
  │
UExChunkSpawner::SpawnNextChunk()
  │
  ├─ 1. AExFloorChunk 생성. (청크 타입별로 직선/곡선 정보 생성 완료)
  │
  ├─ 2. ObstacleManager->SpawnObstaclesOnChunk(Chunk) 호출
  │      └─ 장애물이 청크 표면 Transform 기반으로 모두 스폰 완료 (Race Condition 해소)
  │
  └─ 3. RunnerItemManager->SpawnItemsOnChunk(Chunk, ObstacleManager) 호출
         │
         ├─ 코인 라인 배치 위치(Distance) 순회
         │   ├─ ObstacleManager->QueryObstacleAtDistance()
         │   ├─ Chunk->GetLocalTransformAtDistance() 로 기본 바닥 Z, 곡률 회전값(Roll, Pitch, Yaw) 확보
         │   ├─ CalculateItemZ() 로 장애물 조건 및 포물선(Arc) 높이 합산
         │   └─ SpawnItem()
         │
         └─ AttachToActor(Chunk) (항상 월드 시프트 및 청크 삭제 주기에 동기화)
```

---

## 4. GameplayTag 정의

(기존과 동일 — ExItemTags 네임스페이스)

---

## 5. 멀티플레이 데이터 흐름

(v1.1과 동일 — 클라이언트 사전 예측 `bLocallyPredicted` 사용, 서버 Authority 인증 후 Multicast 처리)

---

## 6. 시스템 상호작용 다이어그램 (v1.2 갱신 - 중앙 제어 방식)

디커플링(Decoupling) 자체는 유지하되, **배치 순서의 무결성을 위해 `ChunkSpawner`를 컨트롤 타워(Control Tower)로 격상**한다.

```
┌──────────────────────────────────────────────────────────────────────┐
│                     AExRunnerGameMode (서버)                          │
│                                                                      │
│  ┌───────────────────────────────┐                                   │
│  │        ChunkSpawner           │     (컨트롤 타워)                   │
│  │                               │          (1) SpawnObstacles()     │
│  │  1. Chunk Actor 스폰          ├──────────────────────────┐        │
│  │  2. 장애물 매니저 명시적 호출  │                          ▼        │
│  │  3. 아이템 매니저 명시적 호출  │  (2) SpawnItems()  ┌────────────┐  │
│  └──────────────┬────────────────┼─────────────────► │ Obstacle   │  │
│                 │                │                 │ Manager    │  │
│                 ▼                │                 └─────┬──────┘  │
│        ┌──────────────────┐      │         QueryContext() │          │
│        │ RunnerItemManager│◄─────┴────────────────────────┘          │
│        │ (CalculateItemZ) │                                          │
│        └────────┬─────────┘                                          │
│                 │                                                    │
│                 ▼ SpawnActor()                                       │
│  ┌───────────────────────────────────────────────────────────────┐   │
│  │        AExItemPickupBase (Replicated, Client Predicted)       │   │
│  │  [클라이언트 예측] 로컬 오버랩 → 즉시 숨김 및 피드백                │   │
│  └───────────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 7. 파일 생성 순서

(v1.1과 대동소이하며, `ExObstacleManager` 및 `ExRunnerItemManager`의 의존성 바인딩 방식이 `GetDelegate`에서 명시적 함수 파라미터 호출로 대체됨)

---

## 8. 검증 체크리스트

### 1단계 핵심 검증 (설계 무결성)
- [ ] `ChunkSpawner`에서 `Obstacles`이 먼저 스폰된 후에만 `Items`를 스폰한다. (Race Condition 방지)
- [ ] 커브형 청크 위에서도 코인이 허공에 뜨거나 파묻히지 않고 표면을 따라 회전하며 배치된다.
- [ ] Gap 장애물 구간에 진입 시, 시작부터 끝 지점까지 코인이 곡선(Jump Arc)을 그리며 배치된다.
- [ ] 로컬 플레이어 오버랩 시 즉시 예측 피드백이 발생하고, Multicast 수신 시 중복 처리되지 않는다.
- [ ] 이펙트(UExItemEffect) 코드 어디에도 상태 변수가 존재하지 않는다.

---

## 변경 이력

| 날짜 | 버전 | 변경 내용 |
|------|------|-----------|
| 2026-03-23 | v1.0 | 초안 작성. Effect-as-Object + Tag-Driven Event 결합 설계 |
| 2026-03-23 | v1.1 | 클라이언트 예측, Stateless 원칙 명시, Assert 보강, 장애물 질의 추가 |
| 2026-03-23 | v1.2 | 중앙 제어식 상호작용 방식 개편 (Race Condition 원천 차단), 점프 궤적 커브(Jump Arc) 확장, 커브형 청크(Curved Chunk) 지형 자동 보간 명시 |
