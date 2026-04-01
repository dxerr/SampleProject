# [버그 리포트] 수동(Manual) 모드에서 좌우 조향 입력 무시 현상

## 이슈 요약
입력 구조를 Strategy 패턴으로 분리 및 개편한 후, 기본 360도 자유 조향 모드인 `EExRunnerInputMode::Manual` 모드 적용 시 위아래 점프/슬라이딩은 정상 동작하나 좌우 터치/조이스틱 조향 등 가로 이동 입력을 내려도 전혀 캡슐을 회전시키지 못하고 무시되는 버그 발생.

## 원인 분석
- 기존에는 `RequestLookAction`(스와이프)과 `NativeOnMoveAction`(플랫폼별 가상 조이스틱 등)가 수동 모드에서 각자 이격을 두고 `OnLookRequested` 이벤트에 붙어서 무브먼트의 회전을 직결했습니다.
- 구조 개편 과정에서 이 두 함수가 모두 Strategy 패턴의 단일 진입점인 `HandleHorizontalInput`으로 통합 라우팅되도록 코드가 수정되었습니다.
- 하지만 `UExRunnerInputStrategy_Manual::HandleHorizontalInput` 함수 내부에 오직 `OnMoveRequested.Broadcast(AxisValue)` 구문만 잔존하고, 무브먼트 컴포넌트 실질적 회전 입력 접수구인 `OnLookRequested` 브로드캐스트 발송 로직이 누락되어버렸기 때문에 캐릭터 회전 타겟(`TargetLookYawOffset`)을 갱신하지 못한 라우팅 단절 결함이었습니다.

## 해결 방법
1. `ExRunnerInputStrategy_Manual.cpp`의 `HandleHorizontalInput` 함수 진입 시 `OnLookRequested.Broadcast(AxisValue.X)`을 한 줄 수동으로 추가하여 빠져버린 델리게이트 발송 체인 완벽 재연결.
2. 이로써 조이스틱 및 폰 화면 스와이프 어느 것으로 입력을 내려도 캡슐 보간 회전(Yaw Offset)의 정상적 트리거로 동작하도록 조향 파이프라인 일원화 및 복구 완료.

## 결과
AutoRun의 자동 3레인 조작 및 분리 로직에는 전혀 영향을 미치지 않으면서 수동 모드 설정 시 캐릭터 좌/우 조향 완벽하게 복구 및 정상화함.
