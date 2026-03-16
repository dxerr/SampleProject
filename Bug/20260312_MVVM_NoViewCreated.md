# MVVM 바인딩 없이 뷰모델 사용 시 널 참조 오류 (Accessed None)

## 📌 개요 (Overview)
- **발생일:** 2026-03-12
- **키워드:** MVVM, ViewModel, Accessed None, Null Reference, Create View Without Bindings, 바인딩
- **관련 파일:** `WBP_ExTouchPad` (또는 바인딩 없이 뷰모델을 사용하는 모든 UI)

## ⚠️ 현상 (Issue)
- 위젯 블루프린트에 ViewModel을 추가(`Create Instance`)하고 C++ 이벤트 연결 목적으로만 사용할 때, 런타임 플레이 시 뷰모델이 `Null` 상태가 되어 `Accessed None` 포인터 참조 에러가 발생함.
- UI 디자이너 Viewmodels 탭 하단에 아래와 같은 노란색 경고 창이 나타남.
  > *"No view will be created for this widget. There are no bindings and there are no events. Your viewmodels won't be initialized."*

## 🔍 원인 (Cause)
언리얼 언리얼 엔진(UE5)의 MVVM 아키텍처는 최적화의 일환으로 **UI 요소와의 실제 데이터 바인딩(View Bindings)이 1개라도 존재하지 않으면, 런타임에 해당 뷰모델을 아예 생성하지 않도록 설계**되어 있습니다. 따라서 블루프린트 그래프에서 이벤트 호출 용도로만 접근하려고 하면 인스턴스가 없어 Null 참조 에러가 납니다.

## 🛠 해결 방법 (Resolution)
1. 에디터 상단 메뉴에서 **`Window` -> `View Bindings`** 패널을 엽니다.
2. 패널 우측 상단의 톱니바퀴 아이콘(Settings)을 클릭하거나 MVVM 관련 옵션에서 **`Create View Without Bindings` (바인딩 없이 뷰 생성)** 항목을 찾아 **체크(활성화)** 합니다.
3. 적용 시 뷰모델 패널 하단의 노란색 경고 문구가 즉시 사라집니다.
4. **Compile & Save** 후 이벤트 그래프에서 뷰모델 Getter(초록색 순수 함수 노드)를 끌어와 연결하면 런타임에 정상 작동합니다.

*(이슈의 유사성을 고려해 `C:\wz\ExFrameWork\Md\Bug\MVVM_EventBinding_Visibility.md` 문서에도 해당 해결책이 동일하게 추가 기록되었습니다.)*
