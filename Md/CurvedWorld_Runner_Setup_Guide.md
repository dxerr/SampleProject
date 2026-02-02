# 커브드 월드 러너 게임 - 구성 가이드

이 문서는 빌드 완료 후 에디터에서 러너 게임을 구성하는 방법을 설명합니다.

---

## 1단계: 바닥 청크 블루프린트 생성

### BP_ExFloorChunk 생성

1. **콘텐츠 브라우저**에서 `/ExCore/Content/BluePrint/` 폴더로 이동
2. **우클릭 → Blueprint Class → AExFloorChunk** 선택
3. 이름: `BP_ExFloorChunk`
4. 블루프린트 열기:
   - **FloorMesh 컴포넌트** 선택
   - **Details → Static Mesh**: 바닥용 메시 선택 (예: `SM_Cube` 또는 커스텀 바닥)
   - **Transform → Scale**: `(10, 10, 0.1)` 정도로 설정 (X: 길이, Y: 너비, Z: 두께)
   - **Material**: 커브드 월드 머티리얼 적용 (나중에 생성)

5. **Class Defaults** 설정:
   - `KillZ`: `-2000` (플레이어 뒤로 이 거리까지 가면 삭제)
   - `ChunkLength`: `1000` (청크 길이, Scale X * 100)

6. **컴파일 및 저장**

---

## 2단계: 게임모드에 청크 스포너 추가

### BP_ExCoreGameMode 수정

1. `/ExCore/Content/BluePrint/BP_ExCoreGameMode` 열기

2. **Components 탭**에서:
   - **Add Component** 클릭
   - **ExChunkSpawner** 검색 후 추가
   - 이름: `ChunkSpawner`

3. **ChunkSpawner 컴포넌트 설정**:
   - `ChunkClass`: `BP_ExFloorChunk` 선택
   - `InitialPoolSize`: `5`
   - `SpawnStartX`: `500` (플레이어 앞에서 시작)
   - `ChunkSpacing`: `1000` (청크 간격 = ChunkLength)
   - `MaxActiveChunks`: `4`

4. **Class Defaults → Runner 카테고리**:
   - `BaseGameSpeed`: `600` (cm/s, 6m/s)
   - `SpeedAcceleration`: `10` (초당 가속)
   - `bRunnerModeEnabled`: `false` (시작 시 비활성화)

5. **Event Graph**에 시작 로직 추가:
   ```
   Event BeginPlay
       → Delay (2초)  // 준비 시간
       → Call StartRunnerGame
       → ChunkSpawner → InitializeSpawner
   ```

6. **컴파일 및 저장**

---

## 3단계: 러너 캐릭터 블루프린트 생성

### BP_ExRunnerCharacter 생성 (SandboxCharacter_CMC 하위)

1. **콘텐츠 브라우저**에서 `/Game/Blueprints/` 폴더로 이동
2. `SandboxCharacter_CMC` 우클릭 → **Create Child Blueprint Class**
3. 이름: `BP_ExRunnerCharacter`
4. 저장 위치: `/ExCore/Content/BluePrint/`

5. 블루프린트 열기 → **변수 추가**:
   - `FakeVelocity` (Vector) - 가상 속도
   - `bConstrainPosition` (Boolean) - 위치 고정 여부
   - `FixedPosition` (Vector) - 고정할 위치

6. **Event Graph** 추가:
   ```
   Event Tick
       [Get GameMode as ExCoreGameMode]
       → Get CurrentGameSpeed
       → Make Vector (X: CurrentGameSpeed, Y: 0, Z: Get Velocity.Z)
       → Set FakeVelocity
       
       [If bConstrainPosition]
       → Set Actor Location (FixedPosition)
   ```

7. **컴파일 및 저장**

---

## 4단계: AnimBP 생성 (SandboxCharacter_CMC_ABP 상속)

### ABP_ExRunner 생성

1. **콘텐츠 브라우저**에서 `/Game/Blueprints/` 폴더로 이동
2. `SandboxCharacter_CMC_ABP` 우클릭 → **Create Child Blueprint Class**
3. 이름: `ABP_ExRunner`
4. 저장 위치: `/ExCore/Content/Animation/`

5. **Blueprint Thread Safe Update Animation** 함수에서:
   - Owning Actor → Cast to BP_ExRunnerCharacter
   - Get FakeVelocity → 이 값을 Trajectory 계산에 사용
   
6. **(선택) FakeVelocity 주입**:
   기존 Trajectory 생성 노드를 찾아서 `FakeVelocity`로 오버라이드

7. **컴파일 및 저장**

---

## 5단계: 레벨 구성

### L_ExRunnerLevel 생성

1. **File → New Level → Empty Level**
2. **Save As**: `/ExCore/Content/Map/L_ExRunnerLevel`

3. **필수 액터 배치**:
   - **Player Start**: `(0, 0, 100)` 위치
   - **Directional Light**: 태양광
   - **Sky Atmosphere**: 하늘
   - **Exponential Height Fog**: 안개 (커브드 월드 효과 강화)

4. **World Settings**:
   - `GameMode Override`: `BP_ExCoreGameMode`
   - `Default Pawn Class`: `BP_ExRunnerCharacter`

5. **저장**

---

## 6단계: 테스트 실행

### 플레이 테스트

1. `L_ExRunnerLevel` 열기
2. **Play (PIE)** 실행
3. 확인 사항:
   - [ ] 바닥 청크가 -X 방향으로 이동하는가?
   - [ ] 새 청크가 자동 스폰되는가?
   - [ ] 캐릭터가 달리는 애니메이션을 재생하는가?
   - [ ] 속도가 시간에 따라 증가하는가?

### 디버그 팁
- **Output Log** 확인: `LogExCoreGameMode`, `LogExFloorChunk`, `LogExChunkSpawner`
- **Show Debug** 옵션으로 속도/거리 표시

---

## 다음 단계 (추후 구현)

1. **MF_ExCurvedWorld**: 커브드 월드 머티리얼 함수
2. **장애물 시스템**: AExObstacle, 스폰 패턴
3. **입력 처리**: 점프, 슬라이드
4. **레인 시스템**: Y축 이동

---

## 빠른 참조

| 클래스 | 역할 |
|--------|------|
| `AExCoreGameMode` | 게임 속도 관리, 청크 스포너 소유 |
| `AExFloorChunk` | 이동하는 바닥 청크 |
| `UExChunkSpawner` | 청크 오브젝트 풀 관리 |
| `BP_ExRunnerCharacter` | 플레이어 캐릭터 (FakeVelocity) |
| `ABP_ExRunner` | 모션 매칭 AnimBP |
