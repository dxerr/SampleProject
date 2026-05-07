# 🔄 재부팅 후 이어서 진행 - Mover 이동속도 개선 TODO

> 작성일: 2026-05-07  
> 관련 컨텍스트 대화 ID: `41faa871-85aa-4b4b-82b6-cfcd92ef6394`

---

## 📋 현재까지 완료된 작업

### 핵심 설계 변경 (코드 수정 완료, 빌드 미확인)

| 파일 | 변경 내용 |
|------|----------|
| `ExRunnerMovementComponent.h` | `UCommonLegacyMovementSettings* CachedLegacySettings` 캐싱 필드 추가, forward declaration 추가 |
| `ExRunnerMovementComponent.cpp` | `#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"` 추가 |
| `ExRunnerMovementComponent.cpp` | `TryInitializeMover()`에서 `CachedLegacySettings` 캐싱 |
| `ExRunnerMovementComponent.cpp` | **`SetVelocityInput` → `SetDirectionalInput`** 으로 전환 (핵심 변경!) |
| `ExRunnerMovementComponent.cpp` | `TickComponent`에서 태그 기반 `CommonLegacySettings.MaxSpeed` 갱신 로직 추가 |

### 변경 이유 (왜 이렇게 바꿨는가)

**기존 방식 (문제):**
```
SetVelocityInput(방향 * 800)
→ EMoveInputType::Velocity 모드
→ RequestedSpeed = min(CommonLegacySettings.MaxSpeed=3013, 800) = 800
→ 항상 800cm/s 목표로 가속 (버프 있어도 없어도 800)
→ CommonLegacySettings.MaxSpeed(3013)는 아무 역할 안 함
```

**새로운 방식 (올바름):**
```
SetDirectionalInput(방향)
→ EMoveInputType::DirectionalIntent 모드  
→ DesiredSpeed = CommonLegacySettings.MaxSpeed × AnalogModifier(1.0)
→ DesiredSpeed = CommonLegacySettings.MaxSpeed (태그에 따라 동적)

TickComponent에서:
→ ServerActiveTags 기반으로 GetMaxSpeedFromTags() 계산
→ CommonLegacySettings.MaxSpeed = TargetMaxSpeed (600 or 1200 등)
→ 서버+클라이언트 모두 동일하게 적용
```

---

## ✅ 재부팅 후 즉시 해야 할 작업

### [ ] 1. 빌드 확인
```
Build_Project.bat 실행 or 에디터에서 컴파일
```
- 컴파일 에러 없는지 확인
- 특히 `CommonLegacyMovementSettings.h` 경로가 올바른지 체크

### [ ] 2. DA_ExRunnerConfig 데이터 확인 (필수!)

에디터에서 `DA_ExRunnerConfig` 에셋 열고 확인:

| 항목 | 현재 값 | 권장 값 | 비고 |
|------|---------|---------|------|
| `Movement.BaseRunningSpeed` | 확인 필요 | **600~800** | 기본 달리기 속도 (버프 없을 때) |
| `TagToSpeedMap[Ex.Buff.SpeedUp]` | 확인 필요 | **1200~1500** | 반드시 BaseRunningSpeed보다 높아야 함! |

> ⚠️ **중요**: `TagToSpeedMap`의 버프 속도가 `BaseRunningSpeed`보다 낮거나 같으면 버프가 적용되지 않음!
> 
> `GetMaxSpeedFromTags`는 `BaseRunningSpeed`에서 시작하여 태그별 속도가 더 클 때만 갱신하는 구조이기 때문.

### [ ] 3. 인게임 테스트

게임 실행 후 화면 좌상단 디버그 로그 확인:

```
[Speed] MaxSpeed=??? | Config=OK | Stat=OK | Tags=(없음)    ← 기본 상태
[Speed] MaxSpeed=??? | Config=OK | Stat=OK | Tags=Ex.Buff.SpeedUp  ← 버프 먹은 후
```

**기대 결과:**
- 기본 상태: `MaxSpeed = BaseRunningSpeed 값` (예: 600)
- 버프 후: `MaxSpeed = TagToSpeedMap[Ex.Buff.SpeedUp] 값` (예: 1200)
- 실제 이동속도(화면 좌하단 숫자)가 버프 전/후로 달라져야 함

### [ ] 4. 로그에서 MaxSpeed 갱신 확인

Output Log에서 검색:
```
[ExRunnerMovement] MaxSpeed 갱신:
```
위 로그가 버프 아이템 획득 시 출력되어야 함 (예: `MaxSpeed 갱신: 600 → 1200`)

---

## 🔍 추가 조사가 필요한 부분

### [ ] 5. BP `ExSandboxCharacter_Mover`의 `WantsToSprint` 처리 확인

- 세 번째 스크린샷의 `Get_Gait` 함수가 `CommonLegacySettings.MaxSpeed`를 변경하는지 확인
- 만약 BP도 MaxSpeed를 변경한다면 우리 코드와 충돌할 수 있음
- 에디터에서 `ExSandboxCharacter_Mover` BP 열어서 `Set Locomotion Or Table` 노드 확인

### [ ] 6. 스프린트(키입력) 달리기와 버프의 우선순위 확인

현재 구조:
- 키입력 달리기 (`WantsToSprint`) → BP가 처리
- 아이템 버프 (`Ex.Buff.SpeedUp`) → 우리 코드가 처리

두 가지가 동시에 발생할 경우 어떻게 동작하는지 확인 필요

---

## 🛠️ 만약 빌드 에러가 발생하면

### include 경로 에러 시
`"DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"` → 올바른 경로:
```
c:\wz\UnrealEngine\Engine\Plugins\Experimental\Mover\Source\Mover\Public\DefaultMovementSet\Settings\CommonLegacyMovementSettings.h
```
Build.cs에서 Mover 모듈이 이미 포함되어 있어야 함.

### FindSharedSettings_Mutable 에러 시
`MoverComponent.h` 543~550번 줄에 template 함수로 선언되어 있음:
```cpp
SettingsT* FindSharedSettings_Mutable() const { return Cast<SettingsT>(...); }
```
`MoverComponent.h`가 include되어 있어야 동작 (현재 이미 include됨)

---

## 📁 수정된 파일 목록

```
c:\wz\ExFrameWork\Plugins\GameFeatures\ExRunnerPlay\Source\ExRunnerPlayRuntime\Components\
├── ExRunnerMovementComponent.h    ← CachedLegacySettings 필드 추가
└── ExRunnerMovementComponent.cpp  ← SetDirectionalInput 전환 + TickComponent MaxSpeed 갱신
```

---

## 📊 전체 데이터 흐름 (수정 후)

```
[아이템 버프 획득]
  → ExItemEffect_Buff (서버)
  → ExGameplayEventSubsystem::BroadcastEvent(TAG_Ex_Buff_SpeedUp)
  → ExRunnerStatComponent::OnSpeedUpBuff (서버)
  → ActivateSprint() → ServerActiveTags.AddTag(TAG_Ex_Buff_SpeedUp) (서버→복제)

[클라이언트에서 복제 수신]
  → ServerActiveTags 업데이트

[매 Tick (서버+클라이언트)]
  → ExRunnerMovementComponent::TickComponent()
  → GetMaxSpeedFromTags(ServerActiveTags) → 1200 (버프 속도)
  → CommonLegacySettings.MaxSpeed = 1200

[Mover 시뮬레이션]
  → ProduceInput: SetDirectionalInput(방향)
  → WalkingMode::GenerateMove: DesiredSpeed = MaxSpeed(1200) × 1.0 = 1200
  → ComputeVelocity: 가속도로 1200cm/s 목표에 도달
  → 실제 이동속도 1200cm/s
```
