# [System] Runner Yaw Limit & Rotation Control 통합 리포트

## 1. 개요
러너 플레이 중 조이스틱 입력을 통한 캐릭터의 좌우(Yaw) 회전 범위를 제한하고, 곡선 경로에서의 안정적인 시선 처리를 구현한 내역입니다.

## 2. 주요 개선 및 수정 사항

### A. 데이터셋 확장 및 입력 제한 (`MaxRunnerYawAngle`)
*   **구현**: `UExGameModeDataSet`에 `MaxRunnerYawAngle`(기본 45도) 및 `RunnerLookSensitivity` 프로퍼티를 추가하여 에디터에서 제어 가능하게 함.
*   **입력 컴포넌트 (`UExRunnerInputComponent`)**:
    *   조이스틱 입력(-1.0 ~ 1.0)에 민감도를 곱해 목표 오프셋(`TargetYawOffset`) 산출.
    *   `FMath::Clamp`를 사용하여 지정된 각도 범위를 절대 넘지 않도록 강제.
    *   **버그 수정**: 기존 누적 로직의 결함으로 인해 모델이 뒤로 도는 현상을 정확한 델타 추출 방식으로 수정하여 해결.

### B. 곡선 경로 연동 및 보간 (`Interpolation Fix`)
*   **문제**: 단순 각도 제한 시 곡선 맵에서 캐릭터가 정면을 바라보지 못하는 문제 발생.
*   **해결**: "스플라인 경로 기준 정면 + 조이스틱 오프셋" 구조로 재설계.
    *   `ExRunnerMovementComponent`: 스플라인 접선 방향으로 캐릭터 회전을 업데이트하되, 입력 컴포넌트의 `GetCurrentYawOffset()`을 합산하여 최종 회전 결정.
    *   **부드러운 복귀**: 입력 중단 시 `FInterpTo`를 통해 시선이 다시 경로 정면(0도)으로 자연스럽게 복귀하도록 구현.

### C. 컴파일 및 내부 구조 최적화
*   **종속성 해결**: `ExRunnerPlayRuntime` 플러그인이 `ExFrameWork` 모듈의 데이터셋을 참조할 수 있도록 `Build.cs` 설정 및 `PublicIncludePaths` 수정.
*   **Reset 기능**: 맵의 기준이 바뀌는 상황을 대비해 `ResetBaseYaw()` 함수 제공.

## 3. 핵심 파일
*   `ExGameModeDataSet.h`
*   `ExRunnerInputComponent.h / .cpp`
*   `ExRunnerMovementComponent.cpp`
*   `ExRunnerPlayRuntime.Build.cs`

## 4. 최종 결과
이제 캐릭터는 곡선 길을 따라 자동으로 정면을 응시하며, 사용자는 조이스틱으로 정해진 각도 범위 내에서만 자유롭게 시선을 좌우로 흔들 수 있습니다. 입력 해제 시 즉시 정면으로 정렬됩니다.
