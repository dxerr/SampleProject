# Mover 기반 멀티플레이어 동기화 오류 현상 및 해결

## 개요
- **버그 이름**: Mover 레인 변경 및 커브 이동 시 클라이언트-서버 동기화(Desync) 오류
- **발생 환경**: 멀티플레이 환경 (Dedicated/Listen Server + Client)
- **증상**: 
  1. 클라이언트에서 레인 이동 조작 시 로컬 클라이언트만 이동하고 서버 캐릭터는 중앙에 머무름.
  2. 직선에서는 그럭저럭 진행되나, 곡선 구간(Curve) 진입 시 서버 캐릭터가 트랙 바깥쪽(직선 방향)으로 크게 이탈하며 로컬 캐릭터와 엄청난 거리 오차를 발생시킴.
  3. Mover가 이유 없이 서버의 X좌표를 엄청난 마이너스 수치(예: -468.255)로 강제 롤백시키는 심각한 Desync 발생.

## 원인 파악 (Troubleshooting)

### 1. 레인 변경 서버 갱신 누락 (직선/전역 오차)
- `AutoButtonRun` 모드에서 물리력을 무시한 즉각적인 이동을 위해 `UExRunnerMovementComponent`의 `TickComponent` 내에서 `SetActorLocation`을 직접 호출함.
- 이때 클라이언트가 버튼을 누르면 클라이언트의 로컬 `CurrentLaneIndex`만 변경될 뿐, 서버로는 레인 변경 사실이 전달되지 않았음. (Mover의 기본 `ProduceInput`을 통과하지 않는 강제 위치 보정이었기 때문)
- 따라서 서버는 영원히 `CurrentLaneIndex = 0` (중앙)을 유지하며 직진만 수행함.

### 2. 커브 방향 오차 (곡선 이탈 현상)
- 곡선을 부드럽게 타기 위해서는 현재 진행 거리인 `CurrentPathDistance` 값을 기반으로 `GS->PathManager`에서 우측 벡터(`PathRight`)를 실시간으로 계산해야 함.
- 그러나 `CurrentPathDistance`를 갱신하는 로직이 오직 **`ProduceInput_Implementation`** 내부에만 존재했음.
- 언리얼 Mover 설계상 `ProduceInput`은 클라이언트 로컬 컨트롤러에서만 실행되므로, **서버의 `CurrentPathDistance`는 0.0으로 고정**되어 있었음.
- 이로 인해 서버는 항상 시작점(0.0) 기준의 직교 좌표계로 오프셋을 계산했고, 커브 구간에서도 곡선 방향이 아닌 출발 시점의 절대 직교 방향으로 캐릭터를 던져버려 이탈하게 됨.

### 3. ExFloorChunk의 잘못된 Dynamic Movement Base 등록 (가장 치명적인 롤백 주범)
- 동적으로 생성되는 바닥 지형(`ExFloorChunk`)의 `SplineMesh` 컴포넌트들이 기존에 `EComponentMobility::Movable` 상태로 스폰 및 부착되고 있었음.
- Mover 시스템은 캐릭터가 밟고 있는 바닥이 `Movable`일 경우, 이를 "움직이는 플랫폼(Dynamic Movement Base)"으로 간주하여 월드 좌표 대신 **해당 바닥을 기준으로 한 로컬 상대 좌표 동기화**를 시도함.
- 클라이언트와 서버 간 스플라인 메시 생성의 미세한 타이밍/곡률 차이가 있을 때 Mover가 이를 심각한 이동 오차로 판단하여 캐릭터의 좌표를 무한정 끌어당기거나 강제로 롤백시키는 치명적인 문제를 일으킴.
## 해결 방법 (Resolution)

1. **레인 갱신 동기화 (RPC 및 리플리케이션)**
   - `CurrentLaneIndex`에 `UPROPERTY(Replicated)` 속성 추가 및 `GetLifetimeReplicatedProps` 적용.
   - 클라이언트에서 좌/우 이동 입력 발생 시 로컬 인덱스를 변경함과 동시에 **`Server_SetLaneIndex`** RPC를 호출하여 서버의 레인 인덱스를 동기화.
   - 결과적으로 서버의 `TickComponent`도 동일한 목표 레인으로 `SetActorLocation`을 실행하게 됨.

2. **거리 갱신 위치 최적화 (서버 공통 계산)**
   - 클라이언트만 갱신하던 `CurrentPathDistance` 로직을 **`TickComponent` 가장 상단**으로 이동.
   - 이제 권한(클라이언트, 서버, 프록시)에 상관없이 매 프레임 모든 객체가 자신의 `ActorLocation`을 바탕으로 `CurrentPathDistance`를 갱신.
   - 서버 역시 현재 위치에 맞는 정확한 곡선 구간의 `PathRight` 벡터를 도출하여, 커브에서도 완벽히 일치하는 궤적을 그리게 됨.

3. **ExFloorChunk 컴포넌트 Mobility 변경 (Dynamic Base 차단)**
   - `ExFloorChunk.cpp` 내에서 스폰되는 모든 `SplineMesh`의 Mobility 속성을 `EComponentMobility::Movable`에서 **`EComponentMobility::Stationary`**로 강제 수정함.
   - 이를 통해 Mover 시스템이 동적으로 생성되는 바닥을 "움직이는 플랫폼"이 아닌 "고정된 월드 지형"으로 인식하게 만들어, 불필요한 상대 좌표 베이스 동기화를 원천 차단하고 서버 롤백 현상을 완전히 해결함.

## 교훈 및 지침 (Lessons Learned)
- Mover의 입력 사이클(`ProduceInput`)을 우회하는 강제 조작(`SetActorLocation` 등) 시에는, **서버도 동일한 의도를 알 수 있도록 상태 값 자체를 반드시 RPC로 전달**해야 한다.
- 이동 궤적이나 디버그 수치 산출에 필요한 `Distance`, `Time` 등의 기준 데이터는, 클라이언트 한 곳에서만 업데이트하면 서버 시뮬레이션에서 엉뚱한 기준점을 사용하게 되므로 공통 Tick 등에서 보장된 상태 갱신이 필요하다.
- **멀티플레이어 Mover/CharacterMovement 환경에서 동적으로 스폰되는 지형지물은 반드시 Mobility를 `Stationary` 혹은 `Static`으로 설정해야 한다.** 그렇지 않으면 엔진이 멋대로 Dynamic Movement Base 동기화를 시도하여 원인을 알 수 없는 기괴한 롤백 지옥에 빠질 수 있다.
