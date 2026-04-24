# Bug Resolution: 데디케이티드 서버 롤백 및 Mover 입력 지연/상태 고정 문제

## Keywords
`Dedicated Server`, `Rubber-banding`, `Rollback`, `MoverComponent`, `Lane Transition`, `Movement Sprint`, `Multiplayer Synchronization`

## Issue Summary
1. **이동(🏃‍♂️) 및 점프 불가 문제**: 자동 이동 상태에서 캐릭터가 '걷기' 모드로 고정되고, 패드/키보드 입력(뛰기, 점프 등)이 무시됨.
2. **레인 즉각 이동 불가 문제**: AutoButtonRun 시 부드러운 핸들링 대신 예전 감각의 즉각 이동(설정상 즉각적 레인 전환)을 요청했으나 천천히 움직임.
3. **데디케이티드 서버 원점 강제 롤백 현상**: 캐릭터 전진 시 1번 청크 끝부분에서 원점으로 계속 끌어당기는 고무줄 현상(Rubber-banding) 발생.

## Troubleshooting & Analysis
1. **입력 상태 고정 원인 파악**: 
   - `ExRunnerMovementComponent` 개발 중 Mover 컴포넌트의 순수 이동만 통제하려는 목적 하에 `InputProducers`를 전부 날리는 `MoverComp->InputProducers.Empty();` 코드가 작성됨. 그 결과 스프린트/점프키 바인딩 데이터인 Mover의 `DefaultProducer`가 전부 증발했음.
2. **레인 즉각 이동 불가 원인 분석**:
   - 기존의 즉각 위치 보정 용도인 `SetActorLocation`이 Mover 시뮬레이션 권한과 충돌을 우려하여 삭제됨. 결과적으로 `FInterpTo`라는 보간 로직에 의존해야만 해 패드 터치 시 예전 감각(빠른 이동)이 불가능했음.
3. **데디케이티드 서버 위치 롤백 원인 집중 분석**:
   - `ExRunnerGameMode::Tick`에서 **`GetPlayerPawn(0)`**(로컬 플레이어 전용)을 기준으로 `GameState->RealPlayerPathDistance`를 갱신 중이었음. 
   - 데디케이티드 서버에는 `Player 0` 모델이 없으므로 항상 전역 진행 거리(Global Path Distance)가 '0'에 가까웠음. 결과적으로 멀리 나아가는 클라이언트 클라이언트 플레이어들의 위치를 강제로 원점(거리 0 지점)으로 위치 교정시켰음.

## Resolution
1. **Mover 입력 통제 정상화 (`ExRunnerMovementComponent.cpp`)**:
   - `InputProducers.Empty()` 구문 삭제. 
   - 억지로 `Inputs.SuggestedMovementMode`를 `Walking`으로 고정하던 로직 해제. (이제 `TargetPawn`가 전달하는 뛰기/걷기 의도를 Mover가 자체적으로 판단)
2. **AutoButtonRun 즉각 보정 복원 (`ExRunnerMovementComponent.cpp`)**:
   - `UpdateLanePosition`에서 `bUseDirectLateralMovement`이 활성화 되어 있으면 경로 직교 매트릭스(PathRight)에 레인 오차(Lateral Error)를 구해 `SetActorLocation`으로 즉가적으로 보정하도록 롤백 조치.
3. **분산식 멀티플레이어 경로 추적 도입 (`ExRunnerMovementComponent.h/cpp` & `PlayerState` / `GameState` / `GameMode`)**:
   - 단일 전역 변수 대신 **각 MovementComponent (`CurrentPathDistance`)가 자신의 독립적 위치를 기준으로 경로 진행 거리를 측정**하게 함.
   - `HasAuthority()`가 참인 상황일 때 각 폰은 자신의 진행 거리를 `AExRunnerPlayerState::UpdatePathDistance()`를 호출하여 서버에 반영.
   - 불필요하고 충돌을 유발했던 `GameMode::Tick`의 전역 0번 플레이어 강제 동기화 소스는 전면 제거.
   - [중요] **Mover 시스템의 서버 미실행 이슈 대응**: `ProduceInput_Implementation`은 서버에서 실행되지 않으므로, 거리 계산(`CurrentPathDistance`) 로직을 서버/클라 모두 실행되는 `TickComponent`로 이동하여 서버가 클라이언트의 진행 거리를 유실하지 않도록 보장.
   - `ExRunnerGameState::Tick`은 접속한 모든 `PlayerState` 중에서 가장 높은 점수(가장 멀리간 유저)를 `CurrentPathDistance (LeadDistance)`로 정하여 월드 맵(청크)를 스폰/디스트로이 하도록 아키텍처 개편.

## Date
2026-04-22
