# 🧱 ExRunner 마이그레이션 백업 체크리스트

다음은 `Phase 7` 마이그레이션 진행 시 UPROPERTY에서 삭제될 항목으로, 에디터 재시작 후 `DA_ExConfig_Runner` 에셋에 새롭게 입력해 주셔야 할 기존 값들의 위치와 목록입니다. 
마이그레이션 핫리로드 전에 에디터에서 아래 항목들의 수치를 **스크린샷으로 찍어두시거나 메모해 두시길 권장**해 드립니다.

---

## 1. ExGameModeDataSet (데이터 에셋)
> **위치:** `Content/` 내에 생성해 두셨던 `DA_ExGameMode` (또는 비슷한 이름의 데이터셋)

- `MaxRunnerYawAngle` (보통 45.0)
- `RunnerLookSensitivity` (보통 1.0)
- `LookInterpSpeed` (보통 8.0)
- `SwipeActivationPercentage` (보통 0.3)
- `AutoRunActionCooldown` (보통 0.3)
- `JumpYawPredictionWeight` (보통 1.0)

---

## 2. ExRunnerInputComponent (캐릭터/플레이어 컨트롤러 블루프린트)
> **위치:** 이 컴포넌트가 부착되어 있는 플레이어 블루프린트의 디테일(Details) 패널

- `RunnerLookSensitivity` (보통 0.5)
- `DefaultInputMode` (Manual 또는 AutoRun 등)

---

## 3. ExRunnerMovementComponent (캐릭터 블루프린트)
> **위치:** 이 컴포넌트가 부착되어 있는 캐릭터 블루프린트의 디테일(Details) 패널

- `LaneWidth` (보통 100.0)
- `LaneChangeSpeed` (보통 10.0)

---

## 4. ExChunkSpawner (게임 모드 또는 게임 스테이트 등)
> **위치:** 청크 스포너가 붙어있는 액터의 디테일 패널

- `bUsePooling` (True/False)
- `InitialPoolSize` (보통 5)
- `SpawnStartX` (보통 0.0)
- `ChunkSpacing` (보통 1000.0)
- `MaxActiveChunks` (보통 10)

---

## 5. ExBeatSyncComponent (컨트롤러 또는 매니저 액터)
> **위치:** 비트 싱크 컴포넌트가 붙어있는 액터의 디테일 패널

- `SpawnProbabilityPerBeat` (보통 0.5)
- `StrongBeatBonus` (보통 0.2)
- `bBeatSyncEnabled` (True/False 여부)

---

### 💡 팁
위 수치들을 모두 백업하셨다면 새로운 컴파일 이후 `DA_ExConfig_Runner` 파일 단 1곳만 열어서 위 수치들을 모두 입력해주시면, 모든 기능이 기존과 100% 동일하게 동작하게 됩니다!
