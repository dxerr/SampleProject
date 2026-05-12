# Sprint 미활성화 문제 디버깅 Todo
> 작성일: 2026-05-11 | 세션: Sprint 자동 활성화 이슈 조사

---

## 🎯 문제 정의

**증상**: 매치 시작 후 MoveSpeed가 ~380 (Run 속도)에 머물고, Sprint 속도 (~649)로 전환되지 않음.

**Expected**: 3초 카운트다운 종료 → Match_Playing 진입 → Sprint 자동 활성화 → MoveSpeed ~649

---

## 🔬 조사로 파악된 구조

### BP 이동 속도 체계 (ExRunnerCharacter_Mover_Child - CharacterMover 컴포넌트)
| 상태 | 속도 (cm/s) |
|------|-------------|
| Walk | 115 |
| **Run** | **376** |
| **Sprint** | **649** |
| Crouch | 230 |

### Sprint 트리거 메커니즘 (BP 스크린샷 분석)
```
IA_Sprint (Triggered)
  → Set members in S_PlayerInputState
      WantsToSprint = true
      WantsToWalk   = false
  → Branch: Is Crouching?
      false → Set members in S_PlayerInputState (WantsToCrouch = false)
```
- Sprint는 **BP 내 S_PlayerInputState 구조체의 WantsToSprint 플래그**로 제어됨
- 속도는 `CommonLegacyMovementSettings->MaxSpeed`가 아닌 **CharacterMover의 MovementMode별 속도값으로 결정됨**
- `ApplySpeedMultiplier()` 방식은 **무의미** (다른 시스템)

### 현재 ProduceInput_Implementation 동작
```cpp
// ExRunnerMovementComponent.cpp L147
FCharacterDefaultInputs& Inputs = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();
// ...DirectionalInput, OrientationIntent만 설정
// WantsToSprint 플래그: 설정하지 않음 → 기본값 false → Run 속도로 동작
```

### 실패한 시도들
| 시도 | 왜 안 됐는가 |
|------|-------------|
| `BeginPlay`에서 `RequestSprintAction(true)` | `IsMatchActive()=false`로 즉시 차단됨 |
| `Match_Playing` 이벤트 → `RequestSprintAction(true)` | `InjectInputBoolForAction`이 서버 측 비로컬 플레이어에서 Enhanced Input으로 전달되지 않음 |
| `ApplySpeedMultiplier(weight)` 직접 호출 | `CommonLegacyMovementSettings->MaxSpeed` 조작 → CharacterMover 속도 체계와 무관 |

---

## ✅ 남은 작업 (Todo)

### [최우선] Sprint 플래그 직접 설정 방식 조사
- [ ] `FCharacterDefaultInputs` 구조체 내 `bWantsToSprint` 필드 유무 확인
  - 엔진 Mover 플러그인에서 `bWantsToCrouch : 1`만 발견됨 → Sprint 없음
  - → **S_PlayerInputState는 BP 커스텀 구조체** 가능성 높음
- [ ] **UnrealMCP 서버 활성화 후** `ExRunnerCharacter_Mover_Child` BP의 ProduceInput / Get_Gait 노드 조사
  - 어떤 방식으로 S_PlayerInputState를 읽어 Gait를 결정하는지 확인
  - S_PlayerInputState가 C++ `FCharacterDefaultInputs`에 포함되는지 확인

### [조사 항목] 부모 클래스 ExSandboxCharacter_Mover 분석
- [ ] `Get_Gait` 함수 전체 로직 파악 (두 번째 스크린샷)
  - WantsToSprint → Sprint Gait 전환 노드 확인
  - ProduceInput에서 S_PlayerInputState를 어떻게 설정하는지 확인
- [ ] `ExSandboxCharacter_Mover` BP 에셋 경로 확인

### [구현 방향] 확인 후 선택할 수정 방안
**Option A**: `ProduceInput_Implementation`에서 `FCharacterDefaultInputs`의 Sprint 관련 필드 직접 설정
```cpp
// FCharacterDefaultInputs에 bWantsToSprint가 있다면:
Inputs.bWantsToSprint = bIsAutoRunMode;  // AutoRun 중엔 항상 Sprint
```

**Option B**: BP ProduceInput Override에서 S_PlayerInputState.WantsToSprint = true 설정
- C++에서 접근 불가능한 경우 BP 노드로 처리

**Option C**: `CharacterMoverComponent`의 `bWantsToCrouch` 방식처럼 별도 플래그 노출
- C++ 측 `UCharacterMoverComponent`의 멤버로 Sprint 상태를 직접 설정

---

## 🔧 MCP 서버 전환 (현재 진행 중)

- [x] `unreal-python-bridge` MCP 비활성화 (문제 있음)
- [ ] `UnrealMCP` 플러그인 MCP 서버 활성화
  - 위치: `c:\wz\ExFrameWork\Plugins\UnrealMCP\MCP\`
  - Python 가상환경: `python_env\Scripts\python.exe` ✅ 존재 확인
  - 포트: 13377 (UE 플러그인 측 소켓 서버)
  - **주의**: UE 에디터가 실행 중이어야 연결 가능

---

## 📋 이전 세션에서 완료된 작업

- [x] UI 속도 미연동 수정: `ExRunnerStatComponent::UpdateStats` MoverComponent 재캐싱 방어 로직 추가
- [x] `AddUnique` 적용: Child Actor 재생성 시 InputProducers 중복 등록 방지
- [x] `ExRunnerStartDiag` 진단 로그 추가: Match 시작 시퀀스 4단계 확인 가능
- [x] `CheckAndStartMatch` Expected 3명 조건 정상 동작 확인
- [x] **멀티플레이어 Sprint 활성화 버그 수정**: C++ `ProduceInput`에서 `MaxSpeed` 직접 제어 구현 (Enhanced Input 주입 한계 극복)
- [x] `SetWantsToSprint` 인터페이스 추가: 버프 및 매치 시작 시퀀스 연동 완료

---

## 🗂️ 관련 파일

| 파일 | 역할 |
|------|------|
| `ExRunnerBuffComponent.cpp` | `Match.Playing` 시작 시 `SetWantsToSprint(true)` 직접 호출 |
| `ExRunnerMovementComponent.cpp` | `ProduceInput`에서 `bWantsToSprint`에 따라 `MaxSpeed` 동기화 (649 / 376) |
| `ExRunnerInputComponent.cpp` | 수동 조작 시 `SetWantsToSprint` 상태 전파 추가 |
| `ExRunnerCharacter_Mover_Child.uasset` | (참고) 기존 BP Sprint 로직은 C++에 의해 대체/보완됨 |
