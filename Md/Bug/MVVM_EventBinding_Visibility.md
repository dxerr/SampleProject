# MVVM View Binding 함수 미노출 문제 (Event Binding Visibility)

## 현상 (Issue)
* UMG의 View Binding 패널에서 위젯 이벤트(예: `On Pressed`, `On Clicked`)를 ViewModel의 함수에 연결(`One Way To View Model`)하려 할 때, 대상 함수(예: `OnJumpButtonPressed`)가 검색 레이어에 노출되지 않음.
* 기존에 바인딩 해두었던 설정에 경고 아이콘(⚠️)이 표시되며 연결이 끊어진 것처럼 보임.
* 대상 함수는 C++ 헤더에 `UFUNCTION(BlueprintCallable)`로 정상 선언되어 있으며, 위젯 델리게이트 시그니처(void 반환, 파라미터 없음)와도 정확히 일치함.

## 원인 (Cause)
* **Binding 옵션 선택 오류**: Button의 Event(예: `On Pressed`)를 ViewModel의 대상 함수로 Delegate 바인딩하려면 콤보박스에서 `Event` 연동 방식을 선택해야 합니다.
* 좌측의 위젯 속성을 선택할 때 이벤트 전용 핀이 아닌 일반 프로퍼티(`Select`) 핀 방식으로 바인딩하려 할 경우, 시그니처가 일치하는 함수나 프로퍼티가 아니므로 검색 목록에 노출되지 않거나 "No field selected" 상태가 됩니다.

## 해결 방법 (Resolution)
1. **Event 핀 확인**: View Binding 패널에서 좌측 위젯의 대상을 고를 때, 반드시 위젯의 이벤트(Event) 항목(예: `On Clicked`, `On Pressed` 등)을 선택해야 합니다.
2. **One Way To View Model**: 방향을 `One Way To View Model`로 설정하고 우측에서 ViewModel의 대상 `UFUNCTION`을 지정합니다.
