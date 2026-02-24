# Bug Report: 러너 트레드밀 시스템 제거 후 발생한 버그 (회전 무시 및 바닥 삭제/생성 불가)

## Issue 1: 커브 진입/진출 시 캐릭터가 스플라인을 무시하고 기존 방향을 유지함
* **원인**: 트레드밀 관련 코드를 정리하는 과정에서, `ExRunnerGameMode`에 존재하던 조향 제어 로직(`UpdateCharacterRotation`) 전체가 실수로 함께 유실되었습니다. 이로 인해 경로(스플라인)의 접선 방향을 확인하고 플레이어 컨트롤러 회전을 갱신하는 코드가 작동하지 않았습니다.
* **해결**: `ExRunnerGameMode.cpp` 파일에 해당 함수를 복구하였습니다. 또한 새롭게 도입된 `ExRunnerMovementComponent`의 레인 시스템(`DesiredLateralOffset`) 값을 가져와, 현재 경로의 중앙선(IdealLocation) 대비 오차 정도(LateralError)에 따라 조향(`SteeringYaw`)이 P-Control로 부드럽게 조정되도록 연동하였습니다.

## Issue 2: 게임 진행 시 진행 방향 앞쪽 바닥청크가 생성되지 않고, 뒤쪽 바닥이 영원히 삭제되지 않음
* **원인**: 바닥 삭제를 관장하는 `AExFloorChunk::Tick`에 구 트레드밀 시절의 동작(좌표축 X를 비교하여 파괴) 잔재가 남아있었습니다. 새 경로 생성 시, 첫 번째 직선 바닥 청크의 `PathDistance`가 0으로 설정되어 있어, 구형 레거시 X-좌표 비교 로직으로 빠졌습니다. 더 이상 트레드밀 월드가 뒤로 이동하지 않으므로 바닥이 `KillZ` 선을 넘지 못해 삭제되지 못했고, 삭제가 일어나지 않으니 새 청크의 스폰(`SpawnNextChunk`) 연쇄 작용도 중단되었습니다.
* **해결**: 트레드밀이라는 개념이 완전히 삭제되었기 때문에 `ExFloorChunk.cpp`에서도 레거시 X 위치 기반 코드를 완전히 배제했습니다. 오로지 가상 경로상의 거리값 구조인 `CachedGameMode->GetPlayerPathDistance() + KillZ` 여부만 판별하도록 단순화하여 릴레이 재생성 루프를 정상화했습니다.

## Issue 3: C++ 중복 함수 선언 컴파일 에러
* **원인**: 라이브러리를 수정하다가 `UpdateCharacterRotation` 함수가 `ExRunnerGameMode.cpp`의 서두와 후미 두 군데에 작성되는 중복 정의 문제가 일어났습니다. 그리고 사용되지 않는 헤더 변수인 `CorrectionStrength`을 참조하여 에러가 퍼졌습니다.
* **해결**: 중복 선언 중 하나를 완전히 제거하고, C++ 구문 에러를 일으키던 빈 `return;` 문들을 다듬었으며, 컴파일을 정상적으로 통과(`Exit Code 0`)시켰습니다.
