# [Issue] 장애물 상호작용 위치 동기화 및 모션 워핑

## 1. 문제 정의 (Problem Definition)

### 1.1 현상
- 플레이어가 트레드밀 위에서 장애물(Climb/Vault 대상)과 상호작용 시, 모션 워핑(Motion Warping)이 장애물의 이동을 추적하지 못함.
- 장애물은 시스템에 의해 뒤로(-X) 이동하고 있지만, 캐릭터의 파쿠르 모션은 상호작용이 시작된 순간의 위치(허공)를 향해 수행됨.
- 결과적으로 캐릭터와 장애물의 위치가 어긋나는 동기화 문제 발생.

### 1.2 원인 분석
- `ExObstacleInteractionComponent`는 충돌 시 `ExRunnerMovementComponent::SetInteractionTarget`을 호출함.
- **구현 누락**: `ExRunnerMovementComponent::SetInteractionTarget` 함수 내부가 비어 있거나 단순히 변수만 저장하고, 실제 모션 워핑 시스템(`MotionWarpingComponent`)에 타겟을 등록하는 로직이 없었음.
- 모션 워핑은 기본적으로 타겟의 위치를 한 번만 캡처하거나, 명시적으로 업데이트해주지 않으면 움직이는 타겟을 추적하지 않음.
- **조기 종료 (Premature Overlap End)**: 트레드밀 시스템에서 장애물이 뒤로 빠르게 이동하므로, 애니메이션이 끝나기도 전에 충돌 박스(Overlap)를 벗어남. 이로 인해 `OnOverlapEnd`가 호출되어 `ClearInteractionTarget`이 실행되고, 워핑 타겟이 제거됨.

## 2. 해결 방안 (Solution Plan)

### 2.1 Treadmill 일시 정지 전략 (New Strategy)
- **개념**: 등반(Climb/Vault) 동작 중에는 트레드밀(World Shift)을 일시 정지시켜, 캐릭터와 장애물 간의 상대 속도를 0으로 유지함.
- **장점**:
    - 모션 워핑 동기화 문제 원천 차단 (타겟이 움직이지 않음).
    - "조기 종료(Premature Overlap End)" 문제 자동 해결 (장애물이 뒤로 도망가지 않음).
    - 복잡한 상대 속도 계산 불필요.

### 2.2 구현 상세 (Implementation Details)

#### A. ExCoreGameMode 수정
- `bTreadmillPaused` 플래그 추가.
- `SetTreadmillPaused(bool bPaused)` 함수 구현.
- `Tick` 함수에서 `bTreadmillPaused`가 true면 `ShiftWorld` 호출 건너뜀.

#### B. ExRunnerMovementComponent 수정
- `SetInteractionTarget`: `GameMode->SetTreadmillPaused(true)` 호출.
- `ClearInteractionTarget`: `GameMode->SetTreadmillPaused(false)` 호출.

#### C. ExObstacleInteractionComponent (Rollback)
- 이전의 "주석 처리한 코드" 복구. (`OnOverlapEnd`에서 `ClearInteractionTarget` 호출).
- 이유: 트레드밀이 멈추면 정상적으로 등반 후 Overlap이 종료되므로, 기존 로직을 그대로 사용해도 안전함.

## 3. 검증 계획 (Verification)
- [ ] 인게임 테스트: 장애물 상호작용 시 배경(바닥) 이동이 멈추는지 확인.
- [ ] 파쿠르 종료 후 다시 배경 이동이 시작되는지 확인.

## 3. 검증 계획 (Verification)
- [x] 컴파일 및 빌드 성공 확인.
- [ ] 인게임 테스트: 트레드밀 속도가 빠른 상태에서 파쿠르 실행 시, 캐릭터가 뒤로 밀리는 장애물에 정확히 손을 짚고 올라가는지 확인.

## 4. 비고
- 이 수정 사항은 이미 로컬 코드에 적용되었으며, 사용자의 최종 승인 후 Git 커밋 예정임.
