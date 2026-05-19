# 버그: Runner Sprint 자동 활성화 실패 (Mover 입력 체계 불일치)

> **해결 완료**: 2026-05-11  
> **원본**: `Todo/Sprint_Activation_Debug_Todo.md`

---

## 증상

매치 시작(`Match.Playing`) 후 이동속도가 Run(~376) 속도에 고정되고 Sprint(~649)로 전환되지 않음.

---

## 원인

### 1. Enhanced Input 주입의 서버 한계
`InjectInputBoolForAction`은 로컬 플레이어의 Enhanced Input 파이프라인에만 전달됨.  
서버 측 비로컬 플레이어에게는 도달하지 않아 Sprint 트리거 불가.

### 2. `SetVelocityInput` 방식의 MaxSpeed 무시
```
SetVelocityInput(방향 * 800)
→ EMoveInputType::Velocity 모드
→ RequestedSpeed = min(MaxSpeed=3013, 800) = 800  ← MaxSpeed가 아무 역할을 못 함
→ 버프가 있어도 없어도 항상 800cm/s 목표
```
Mover의 `CommonLegacyMovementSettings.MaxSpeed`를 동적으로 바꿔도 `SetVelocityInput` 방식에서는 반영되지 않음.

### 3. BP의 S_PlayerInputState와 C++ ProduceInput 분리
BP 내 Sprint 로직은 커스텀 구조체 `S_PlayerInputState.WantsToSprint`로 Gait를 제어함.  
C++ `ProduceInput_Implementation`은 이 플래그를 설정하지 않아 항상 Run Gait로 동작.

---

## 해결

### SetDirectionalInput 방식으로 전환
```
SetDirectionalInput(방향)
→ EMoveInputType::DirectionalIntent 모드
→ DesiredSpeed = CommonLegacyMovementSettings.MaxSpeed × 1.0
→ MaxSpeed를 태그 기반으로 동적 갱신하면 즉시 반영됨
```

### SetWantsToSprint 인터페이스 추가
C++ `ProduceInput_Implementation`에서 `bWantsToSprint` 상태에 따라 `MaxSpeed`를 직접 설정:
- Sprint 상태: `MaxSpeed = 649`
- Run 상태: `MaxSpeed = 376`

### 매치 시작 시 자동 활성화
`ExRunnerBuffComponent`에서 `Match.Playing` 이벤트 수신 시 `SetWantsToSprint(true)` 직접 호출.  
Enhanced Input 주입 방식을 완전히 우회.

---

## 수정 파일

| 파일 | 변경 내용 |
|------|-----------|
| `ExRunnerMovementComponent.h/cpp` | `SetWantsToSprint()` 인터페이스 추가, `SetDirectionalInput` 전환, Tick에서 MaxSpeed 동기화 |
| `ExRunnerBuffComponent.cpp` | `Match.Playing` 시 `SetWantsToSprint(true)` 호출 |
| `ExRunnerInputComponent.cpp` | 수동 조작 시 `SetWantsToSprint` 상태 전파 |

---

## 교훈

- **Mover에서 속도 제어가 필요하면 `SetDirectionalInput` + `MaxSpeed` 조합을 사용**할 것. `SetVelocityInput`은 MaxSpeed를 무시한다.
- **멀티플레이어 Sprint/Gait 제어는 Enhanced Input 주입 방식이 아닌 상태값 복제(Replicated) 방식**으로 처리해야 서버-클라이언트 동기화가 보장된다.
