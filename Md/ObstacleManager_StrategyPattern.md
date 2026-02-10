# 장애물 매니저 고도화 — Strategy Pattern 리팩토링

> 작성일: 2026-02-10
> 대상: `UExObstacleManager::SpawnObstaclesOnChunk`

---

## 1. 배경 및 문제점

기존 `SpawnObstaclesOnChunk`는 모든 `EExObstacleType`을 **하나의 로직**으로 처리:
- Gap/WallRun/Climb/Slide 각각 다른 배치 방식이 필요하지만 분기 없음
- 새 타입 추가 시 함수가 비대해지는 구조

## 2. 적용 디자인 패턴: Strategy Pattern (전략 패턴)

### 핵심 개념
- 타입별 알고리즘을 **별도 클래스로 캡슐화**
- `TMap<EExObstacleType, Strategy*>`으로 **1:1 매핑**
- 공통 로직은 Manager에 유지, **세부 로직만 Strategy에 위임**

### 구조도

```
UExObstacleManager (공통 로직)
  ├── SelectRandomDefinition()    ← 랜덤 선택 (공통)
  ├── CheckFeasibility()          ← 배치 가능성 (공통)
  ├── GetObstacleFromPool()       ← 풀링 관리 (공통)
  │
  └── TMap<EExObstacleType, UExObstacleSpawnStrategy*>
        ├── Gap   → UExObstacleStrategy_Gap
        ├── WallRun → UExObstacleStrategy_WallRun
        ├── Climb → UExObstacleStrategy_Climb
        └── Slide → UExObstacleStrategy_Slide
```

### 위임 분리

| 단계 | 처리 주체 | 함수 |
|------|-----------|------|
| 랜덤 선택 | Manager (공통) | `SelectRandomDefinition()` |
| 전략 탐색 | Manager (공통) | `SpawnStrategies.Find()` |
| 배치 가능성 | Manager (공통) | `CheckFeasibility()` |
| **위치 계산** | **Strategy (위임)** | `CalculateSpawnPosition()` |
| 풀링 | Manager (공통) | `GetObstacleFromPool()` |
| **스케일/크기** | **Strategy (위임)** | `ConfigureObstacle()` |
| Interaction | Manager (공통) | `InteractionComp` 설정 |
| 어태치 | Manager (공통) | `AttachToActor()` |
| **복귀 거리** | **Strategy (위임)** | `GetRecoveryDistance()` |

---

## 3. 변경된 파일 목록

### 신규 생성 (ExRunnerPlay/Data/)

| 파일 | 역할 |
|------|------|
| `ExObstacleSpawnStrategy.h/cpp` | 추상 베이스 클래스 (BlueprintNativeEvent) |
| `ExObstacleStrategy_Gap.h/cpp` | Gap 전략 (바닥 숨김, 허공 배치) |
| `ExObstacleStrategy_WallRun.h/cpp` | WallRun 전략 (측면 벽, Y축 오프셋) |
| `ExObstacleStrategy_Climb.h/cpp` | Climb 전략 (머리 높이, Z축 중심) |
| `ExObstacleStrategy_Slide.h/cpp` | Slide 전략 (낮은 천장, MaxPassHeight) |

### 수정 파일

| 파일 | 변경 내용 |
|------|-----------|
| `ExObstacleManager.h` | `TMap<EExObstacleType, Strategy*>` 추가, `SelectRandomDefinition()` 추가 |
| `ExObstacleManager.cpp` | `SpawnObstaclesOnChunk` Strategy 위임 리팩토링 |
| `ExObstacleDefinition.h` | 타입별 조건부 프로퍼티 추가 (`EditConditionHides`) |

---

## 4. 주요 설계 결정

### Strategy 클래스 위치
- **ExRunnerPlay** 모듈에 배치 (장애물은 Runner 전용이므로 Core 불필요)

### BP 대응
- `BlueprintNativeEvent`로 선언 → C++ 기본 구현 + BP 오버라이드 모두 지원
- `Blueprintable` + `EditInlineNew` → BP 서브클래스 생성 및 에디터 인라인 편집 가능

### Type별 프로퍼티 조건부 노출
```cpp
UPROPERTY(EditAnywhere, meta = (
    EditCondition = "Type == EExObstacleType::Gap",
    EditConditionHides))
bool bDisableFloorMesh = true;
```
- `EditCondition`: 조건 충족 시에만 편집 가능
- `EditConditionHides`: 조건 미충족 시 **완전 숨김** (Greyed-out이 아님)

### 타입별 전용 파라미터

| Type | 전용 프로퍼티 | 기본값 |
|------|---------------|--------|
| Gap | `bDisableFloorMesh` | `true` |
| Slide | `MaxPassHeight` | `120 cm` |
| WallRun | `WallHeight` | `300 cm` |
| Climb | `ClimbHeight` | `400 cm` |

---

## 5. 확장 가이드

새로운 장애물 타입 추가 시:

1. `EExObstacleType`에 새 값 추가
2. `UExObstacleSpawnStrategy`를 상속한 새 Strategy 클래스 생성 (C++ 또는 BP)
3. `ConfigureObstacle`, `CalculateSpawnPosition` 구현
4. 에디터에서 `ExObstacleManager`의 `SpawnStrategies` TMap에 새 타입-전략 매핑 추가
5. (선택) `ExObstacleDefinition.h`에 새 타입 전용 파라미터 추가
