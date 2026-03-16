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

---

# MVVM 바인딩 없이 뷰모델 사용 시 널 참조 오류 (Accessed None / No view will be created)

## 현상 (Issue)
* 뷰모델을 추가하고 UI 바인딩 없이 C++ 이벤트 연동용으로만 사용할 때 런타임에 뷰모델을 찾지 못하고 `Accessed None` 에러가 발생함.
* 뷰모델 디자이너 패널 하단에 "No view will be created for this widget. There are no bindings and there are no events. Your viewmodels won't be initialized." 라는 경고 문구가 표시됨.

## 원인 (Cause)
* 언리얼 엔진의 MVVM 시스템은 최적화를 위해 위젯에 연결된 **바인딩(Binding)이 단 하나도 없으면 런타임에 뷰모델 인스턴스를 생성(초기화)하지 않습니다.**

## 해결 방법 (Resolution)
1. 상단 메뉴 **Window -> View Bindings** 패널을 엽니다.
2. 패널 우측 상단의 톱니바퀴 아이콘(Settings) 또는 MVVM 설정에서 **`Create View Without Bindings`** (바인딩 없이 뷰 생성) 옵션을 체크(True)합니다.
3. 설정 후 경고 문구가 사라지는지 확인하고 언리얼 에디터에서 컴파일 및 저장합니다.
4. 이후 이벤트 그래프에서 해당 뷰모델의 Getter 노드(초록색)를 가져와 사용할 수 있습니다.
