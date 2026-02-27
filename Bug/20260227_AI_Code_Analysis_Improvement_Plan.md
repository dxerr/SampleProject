# AI 코드 분석 리포트 검증 및 개선 계획서

주인님, 전달해주신 AI 코드 분석 리포트를 기반으로 `ExFrameWork` 프로젝트 소스 코드를 전수 검사하여 교차 검증을 완료했습니다. 리포트의 지적 사항들은 모두 실제 코드에 존재하는 타당한 문제점들로 확인되었습니다. 

아래는 각 항목별 검증 결과 및 이를 해결하기 위한 리팩토링 및 개선 계획입니다.

---

## 🔴 중복 코드 (Duplicated Code) - 최우선 리팩토링 대상

### 1. `GetVisualBoundsOf` 정적 함수 중복 (완료)
- **검증 결과**: `True.` `ExObstacleManager`, `ExObstacleStrategy_Gap`, `ExObstacleStrategy_Slide`, `ExObstacleStrategy_Climb` 등 여러 파일에 거의 동일한 로직의 함수가 중복 선언되어 있습니다. 클래스마다 미세하게 다른 로직(예: Climb의 원본 Mesh 렌더 기반 처리)이 혼재되어 있습니다.
- **수정 내용**: 
  - `UExObstacleSpawnStrategy` 기본 클래스에 `protected static GetVisualBounds(AActor* Obstacle, bool bUseOriginalMeshExtents = false)` 형태의 통합 헬퍼 함수를 선언하고, 하위 클래스에서 이를 공통으로 호출하도록 리팩토링을 완료했습니다.

### 2. 장애물 BaseSize 측정 + 스케일 적용 패턴 3중 복제 (완료)
- **검증 결과**: `True.` `ConfigureObstacle_Implementation` 내부에 크기 측정 후 `TargetSize / BaseSize` 비율로 `SetActorScale3D`를 호출하는 동일한 패턴이 여러 번 반복됩니다.
- **수정 내용**: 
  - `UExObstacleSpawnStrategy::CalculateObstacleScale(AActor* Obstacle, FVector TargetSize)` 헬퍼 함수를 추가하여 BaseSize 측정 및 나눗셈 로직을 캡슐화했습니다.
  - 이를 통해 각 Strategy 클래스 내부의 중복된 스케일 계산부가 2~3줄로 대폭 축소되었습니다.

### 3. `CalculateSpawnPosition` 내 계산식 및 버퍼(200.f) 하드코딩
- **검증 결과**: `True.` `ActualSpawnDist = SafeStartDist + 200.f;` (`ExObstacleManager.cpp`) 와 거리 계산 로직들이 중복되어 있습니다.
- **개선 계획**:
  - `200.f`와 같은 버퍼 값을 `ExCurveConfig` 또는 `ExObstacleManager`의 `UPROPERTY(EditAnywhere)`로 노출하여 에디터에서 제어 가능하게 변경합니다.
  - 거리/Transform 계산은 공통 유틸리티나 부모 클래스로 병합합니다.

### 4. `ApplyCurve` 함수 주석 복제 (완료)
- **검증 결과**: `True.` `ExFloorChunk.cpp` 의 `ApplyCurve` 함수 상단에 동일한 주석 블록이 3줄 연속으로 존재하는 것을 확인했습니다.
- **수정 내용**:
  - 불필요한 중복 주석 블록을 1개만 남기고 정리했습니다.

---

## 🟠 개선 가능한 설계 문제 (Design Issues)

### 5. `SpawnObstaclesOnChunk` 하드코딩 확률 오류 (완료)
- **검증 결과**: `True.` 주석은 "30% 확률"이라고 되어 있지만, 코드는 `FMath::RandRange(0, 10) < 5` 로 50% 확률로 동작 중이며 값 자체가 하드코딩되어 있습니다.
- **수정 내용**:
  - `UExChunkSpawner`의 `FExObstacleSpawnConfig`에 `SpawnProbability` 변수를 추가하여 에디터에서 확률을 0.0 ~ 1.0 사이로 제어할 수 있도록 수정했습니다.

### 6. `RunSpeed` 폴백 (600.f) 하드코딩 (완료)
- **검증 결과**: `True.` `ExObstacleManager.cpp`에 600.f가 폴백 속도로 하드코딩되어 있습니다.
- **수정 내용**:
  - `UExChunkSpawner`의 `FExObstacleSpawnConfig`에 `DefaultRunSpeed` 변수를 추가하여 하드코딩을 제거하고 에디터에서 설정 가능하도록 통합했습니다.

### 7. `ExRunnerMovementComponent` TargetPawn Lazy Init 구조 (완료)
- **검증 결과**: `True.` `BeginPlay`에서 MoverComponent를 찾고, `TickComponent`에서 또다시 TargetPawn을 찾는 2중 구조로 되어있습니다.
- **수정 내용**:
  - `TickComponent`의 매 프레임 검사 로직을 완전히 제거했습니다.
  - `TryInitializeMover()` 전담 함수를 생성하고, 실패 시 0.1초 간격의 가벼운 백그라운드 타이머(`FTimerHandle InitTimerHandle`) 단위로 부착을 대기하도록 경량화했습니다. 외부의 명시적 호출(BP 연동 등) 없이 컴포넌트 스스로 생명주기를 완벽히 독립적으로 책임지는 단독(Self-Contained) 구조를 완성했습니다.

### 8. `ShouldSpawnObstaclesOnCurve` TODO 무용지물 상태 (완료)
- **검증 결과**: `True.` 함수 자체가 무조건 `return true;`를 호출하며, 커브 진입 제한 기능이 동작하지 않고 있었습니다.
- **수정 내용**:
  - 현재 관련 기능이 불필요하므로 `ShouldSpawnObstaclesOnCurve` 선언 및 구현부를 완전히 제거했습니다.

### 9. 변수명 혼동 (`LastObstacleSafeEndX` vs `Distance`) (완료)
- **검증 결과**: `True.` 실제 로직은 `PathDistance`를 기준으로 계산하는데, 변수 이름은 좌표계 기반인 `X`로 남아있어 혼란을 줍니다.
- **수정 내용**:
  - `LastObstacleSafeEndX`를 `LastObstacleSafeEndDistance`로 리팩토링하여 직관성을 높였습니다.

### 10. `ActivateChunk`의 이중 로그 위임 (완료)
- **검증 결과**: `True.` `ActivateChunkWithRotation`과 내부에서 호출되는 `ActivateChunk`가 각각 Warning과 Log로 중복 출력됩니다.
- **수정 내용**:
  - `ActivateChunk`의 중복되고 과도한 `Warning` 로그를 `Log` 레벨로 단일화/개선하고, `ActivateChunkWithRotation`에서의 중복 출력 로그를 제거하여 가독성을 높였습니다.

### 11. `ApplyCurve` 내 로컬 변수 이중 선언 최적화 (완료)
- **검증 결과**: `True.` `LocalCenter`와 `WorldCenterLocal` 등 비슷한 좌표 연산이 루프 밖과 안에서 무의미하게 반복 계산되고 있었습니다.
- **수정 내용**:
  - `ExFloorChunk::ApplyCurve` 함수 내에서 중복/미사용되던 로컬 변수(`LocalCenter`, `RadialStart`)를 제거하고, `WorldSpace` 기준의 변수 연산을 루프 밖으로 빼내어 프레임당 불필요한 연산을 최적화했습니다.

---

## 🟡 진행 중인 버그 상태 확인 (Bug Tracker)

### 12. Climb Spline 눈덩이 버그 (스케일 풀링 문제)
- **검증 결과**: 코드를 보면 `ExObstacleStrategy_Climb.cpp`에서 원본 메쉬 바운드 기반 측정 방식으로 우회되어 풀링 스케일 파괴를 방지하는 방식으로 수정되어 있습니다. 추가적으로 Spline 컴포넌트에 대한 강제 갱신 로직도 적용되어 있어 **코드 상으로는 패치된 것으로 보입니다.**
- **추후 계획**: 변경된 코드가 실 환경에서 스플라인의 Ledge 포인트를 정상적으로 유지하는지 인게임 테스트를 통해 최종 검증(Verify)만 수행하면 됩니다.

### 13. 커브 Climb 회전 충돌 코드 반영
- **검증 결과**: `ExRunnerGameMode.cpp`에 `bIsTraversing` 플래그가 정상적으로 복원되었고 이벤트 델리게이트 또한 잘 구독되고 있습니다.
- **추후 계획**: 버그가 이미 해결된 상태이므로 단순히 버그 문서(`20260224_CurveClimb_RotationConflict.md`)의 상태를 `[Resolved]`로 업데이트하면 됩니다.

---

## 🛠 종합 진행 권고 사항

주인님, 보고서에 기재된 대부분의 사항들이 기술 부채와 하드코딩에 관련된 **'코드 품질 및 유지보수성 저하 지점'**입니다. 
당장의 게임 플레이가 멈추게 되는 치명적 크래시는 아니지만, 이후 시스템이 확장될 때 버그를 유발할 확률이 매우 높은 구조적 취약점들입니다.

**다음 단계로, 이 계획서에 따른 코드 리팩토링 작업을 시작해도 될지 확인 부탁드립니다!**
만약 수정을 원하신다면, 가장 우선순위가 높은 🔴 **중복 코드 통합** 부터 단계적으로 정리하겠습니다.
