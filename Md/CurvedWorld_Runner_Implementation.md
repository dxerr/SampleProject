# 커브드 월드 러너 게임 - 구현 계획서 (수정본)

## 프로젝트 정보
- **엔진**: Unreal Engine 5.7
- **베이스**: AnimationSample 프로젝트 (SandboxCharacter 기반)
- **캐릭터**: UEFN_Mannequin 리타겟 기반 자체 캐릭터
- **가이드라인**: [ExFrameWork_Guidelines.md](file:///c:/wz/ExFrameWork/Md/ExFrameWork_Guidelines.md) 준수

---

## 기존 에셋 분석 결과

### 사용할 기존 에셋
| 에셋 | 경로 | 용도 |
|------|------|------|
| SandboxCharacter_CMC | `/Game/Blueprints/` | 베이스 캐릭터 BP |
| SandboxCharacter_CMC_ABP | `/Game/Blueprints/` | 베이스 AnimBP |
| AExCoreGameMode | ExCore 플러그인 | 게임모드 확장 |
| CHT_PoseSearchDatabases_Sparse | MotionMatchingData/ | 모션 매칭 DB |

---

## 구현 단계별 계획

### 1단계: 머티리얼 (커브드 월드)

#### [NEW] MF_ExCurvedWorld 머티리얼 함수
- WPO 기반 Z축 오프셋: `Offset.Z = -CurvatureStrength * Distance²`
- MPC_ExCurvedWorld로 곡률 제어

---

### 2단계: 게임 로직 (AExCoreGameMode 확장)

#### [MODIFY] [ExCoreGameMode.h](file:///c:/wz/ExFrameWork/Plugins/GameFeatures/ExCore/Source/ExCoreRuntime/GameModes/ExCoreGameMode.h)

추가할 멤버:
```cpp
// 러너 게임 속도 관리
UPROPERTY(EditDefaultsOnly, Category = "Runner")
float BaseGameSpeed = 600.f;

UPROPERTY(EditDefaultsOnly, Category = "Runner")  
float SpeedAcceleration = 10.f;

UPROPERTY(BlueprintReadOnly, Category = "Runner")
float CurrentGameSpeed;

UPROPERTY(BlueprintReadOnly, Category = "Runner")
float TotalDistance;
```

---

### 3단계: 캐릭터 (SandboxCharacter_CMC 기반)

#### [NEW] BP_ExRunnerCharacter (SandboxCharacter_CMC 하위 액터)

**구조:**
```
SandboxCharacter_CMC (기존 베이스)
└─ BP_ExRunnerCharacter (하위 BP)
   ├─ FakeVelocity (Vector 변수)
   ├─ bConstrainToPlane = true
   ├─ CurrentLane = 1 (추후 확장용)
   └─ Tick: FakeVelocity 업데이트
```

**검토 완료**: SandboxCharacter_CMC는 AnimationSample 기반이며, Lyra와 무관하므로 하위 액터 방식으로 문제 없음.

---

### 4단계: AnimBP (SandboxCharacter_CMC_ABP 상속)

#### [NEW] ABP_ExRunner (SandboxCharacter_CMC_ABP 상속)

**구조:**
```
SandboxCharacter_CMC_ABP (기존 노드 구성 유지)
└─ ABP_ExRunner (상속)
   └─ Thread Safe Update에서 FakeVelocity → Trajectory 주입
      └─ PoseHistoryCollector.SetTrajectory(FakeTrajectory)
```

**검토 완료**: 상속 방식으로 기존 Motion Matching/PoseSearch 노드가 그대로 작동하며, FakeVelocity 주입만 추가하면 됨.

---

### 5단계: 월드 청크 스폰 시스템

#### [NEW] AExFloorChunk (이동하는 바닥 청크)

**트레드밀 메커니즘:**
```
┌─────────────────────────────────────┐
│          월드 이동 방향             │
│  ←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←  │
│                                     │
│  [청크3] [청크2] [청크1] [카메라]  │
│     ↑        ↑        ↑     ▲      │
│     │        │        │     │      │
│   스폰    이동중    곧삭제  플레이어│
└─────────────────────────────────────┘
```

**AExFloorChunk 구현:**
```cpp
UCLASS()
class AExFloorChunk : public AActor
{
    UPROPERTY(EditAnywhere)
    TObjectPtr<UStaticMeshComponent> FloorMesh;
    
    // Tick에서 -X 방향 이동
    virtual void Tick(float DeltaTime) override
    {
        FVector Offset(-CurrentGameSpeed * DeltaTime, 0, 0);
        AddActorWorldOffset(Offset);
        
        // KillZ 도달 시 풀로 반환
        if (GetActorLocation().X < KillZ)
            ReturnToPool();
    }
};
```

#### [NEW] UExChunkSpawner (청크 스폰 매니저)

**스폰 로직:**
1. 초기화 시 3~5개 청크 미리 생성 (오브젝트 풀)
2. 카메라 전방 일정 거리에 청크 배치
3. 청크가 KillZ 도달하면 풀로 반환 후 재배치
4. 장애물은 청크에 Attach하거나 별도 풀로 관리

**레벨 구성:**
```
L_ExRunnerLevel (새 레벨)
├─ PlayerStart (X=0, Y=0, Z=100)
├─ DirectionalLight
├─ SkyAtmosphere
├─ ExponentialHeightFog (커브드 월드 효과 강화)
└─ AExCoreGameMode (월드 세팅에서 지정)
```

---

### 6단계: 장애물 & 수집 아이템

#### [NEW] AExObstacle (장애물 베이스)
- 점프/슬라이드로 회피
- 오브젝트 풀링 지원

#### [NEW] AExCollectible (수집 아이템)
- 코인, 파워업 등
- 오버랩 시 수집 처리

---

## 명명 규칙 체크리스트

| 항목 | 명명 | 준수 |
|------|------|------|
| 게임모드 | AExCoreGameMode (기존) | ✅ |
| 캐릭터 BP | BP_ExRunnerCharacter | ✅ |
| AnimBP | ABP_ExRunner | ✅ |
| 바닥 청크 | AExFloorChunk | ✅ |
| 스포너 | UExChunkSpawner | ✅ |
| 머티리얼 함수 | MF_ExCurvedWorld | ✅ |

---

## 작업 순서 (권장)

1. ✅ 명세서 검토 및 분석 완료
2. ⬜ BP_ExRunnerCharacter 생성 (SandboxCharacter_CMC 하위)
3. ⬜ ABP_ExRunner 생성 (SandboxCharacter_CMC_ABP 상속)
4. ⬜ FakeVelocity → Trajectory 주입 로직 구현
5. ⬜ AExCoreGameMode에 러너 속도 관리 추가
6. ⬜ MF_ExCurvedWorld 머티리얼 함수 생성
7. ⬜ AExFloorChunk / UExChunkSpawner 구현
8. ⬜ L_ExRunnerLevel 테스트 레벨 구성
