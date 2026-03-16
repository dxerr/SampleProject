# 러너 게임 Yaw 회전 제한 기능 개선

## 개요
- **요청 사항**: 모바일 조이스틱을 통한 좌우(Yaw) 회전 시, 0을 기준으로 -1~1 비율에 따라 최대 회전 각도를 제한하도록 개선. 지정된 최대 각도에 도달하면 더이상 회전(Add)이 되지 않도록 처리.
- **관련 파일**:
  - `ExGameModeDataSet.h`
  - `ExRunnerInputComponent.h`
  - `ExRunnerInputComponent.cpp`

## 구현 내용
1. **데이터셋 확장 (`UExGameModeDataSet`)**:
   - `MaxRunnerYawAngle`: 최대 러너 좌우 이동 각도 제한 (기본 45도) 추가.
   - `RunnerLookSensitivity`: 조이스틱 입력 민감도 조절용 프로퍼티 추가.

2. **입력 컴포넌트 개선 (`UExRunnerInputComponent`)**:
   - `BaseYaw`: 최초 게임 시작 시 폰이 바라보는 기준 앞쪽 회전값 저장.
   - `CurrentYawOffset`: 누적된 Yaw 회전량을 기록.
   - 외부에서 `GameModeDataSet` 레퍼런스를 할당받아 최대 각도와 민감도를 사용할 수 있도록 구조 개편 (할당되지 않았을 경우를 위해 컴포넌트 자체 변수도 Fallback으로 제공).
   - `RequestLookAction` 함수 내에서 누적될 `NextYawOffset`을 계산하고, 이 값이 `MaxRunnerYawAngle`을 초과하거나 미달하면 한계점까지만 적용되도록 제한(Clamp) 처리 수행.
   - 제한된 실질적인 델타 각도만 `OnLookRequested` 이벤트로 브로드캐스트하여 BP의 `AddControllerYawInput`이 제어 한계를 넘지 않도록 안전망 구축.
   - 맵이 구부러지는 등 정면의 기준이 바뀌어야 하는 상황을 대비해 블루프린트에서 호출 가능한 `ResetBaseYaw()` 함수 추가.

## 주의 사항
- 블루프린트에서 `UExRunnerInputComponent`를 셋업할 때, `GameModeDataSet` 프로퍼티에 `DA_ExGameModeDataSet` 에셋을 지정해주면 해당 데이터셋의 값을 사용하게 됩니다.
- 에셋이 지정되지 않더라도 컴포넌트 디테일 패널에서 `MaxRunnerYawAngle` 및 `RunnerLookSensitivity` 값을 직접 수정할 수 있습니다.
