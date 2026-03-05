# ExCore 모던 UI 시스템 매뉴얼 (에디터 셋업 가이드)

본 문서(`ExFrameWork_UI_Setup_Guide.md`)는 코드로 구현된 `ExFrameWork_Modern_UI_Architecture.md`의 구조를 엔진 에디터상에서 최종 활성화하기 위해 **주인님이 직접 진행하셔야 하는 수동 설정 가이드**입니다.

## 1. 필수 플러그인 활성화
- **Common UI Plugin**: 활성화 후 에디터 재시작
- **UI Model View View Model**: 활성화 상태 확인 (최신 UE5엔 기본 활성화 경향)

## 2. 프로젝트 세팅 (Project Settings)
1. **Engine - General Settings**
   - `Game Viewport Client Class` 항목을 기본 `GameViewportClient`에서 **`CommonGameViewportClient`**로 반드시 변경해야 입력 라우팅 시스템이 정상 동작합니다.
2. **Game - Common Input Settings**
   - (주의: `Plugins - Common UI Input Settings`가 아닌, 좌측 스크롤 베이스의 `Game` 카테고리에 있는 `Common Input Settings`를 클릭하셔야 합니다)
   - `Input Data`: **`CommonUIInputData`** 베이스를 상속받은 블루프린트 클래스(Blueprint Class)를 새로 생성하여 할당. (주의: Data Asset이 아닙니다)
     - (※ 블루프린트 빈 공간 우클릭 -> Blueprint Class -> All Classes 검색창에 `CommonUIInputData` 검색 후 생성하여 `BP_ExCommonUIInputData`로 명명합니다)
     - **[BP_ExCommonUIInputData 세팅 가이드]**
       1. 생성한 블루프린트를 더블 클릭하여 엽니다.
       2. 디테일 패널을 보면 여러 가지 액션을 지정할 수 있는 칸이 있습니다.
       3. **Enhanced Input Click Action**: UI에서 버튼을 누를 때 사용할 입력 액션(예: `IA_UI_Confirm` 등)을 만들어 넣습니다.
       4. **Enhanced Input Back Action**: 뒤로가기(취소) 시 사용할 입력 액션(예: `IA_UI_Cancel` 등)을 만들어 넣습니다.
       5. (선택) 하위 호환성을 위해 `Default Click Action`, `Default Back Action`에 기존 레거시 데이터 테이블 액션을 넣을 수도 있으나, Enhanced Input을 쓴다면 위 두 개 항목이 가장 중요합니다.
       6. 입력을 할당했다면 저장하고 창을 닫습니다.
   - `Default Input Action DataBase`: 만약 5.1~5.3처럼 해당 옵션이 존재한다면 **`CommonInputActionDataBase`** 베이스 에셋을 생성하여 할당(내부 Row에 `UI.Action.Cancel` 추가). 5.4 이상부터는 생략되거나 다른 방식으로 병합되었을 수 있으므로 옵션이 보이지 않으면 무시하셔도 좋습니다.

## 3. UI 매니저 스크립트 연결 확인 (블루프린트 계층)
C++ Base로 작성된 `UExHUDLayoutWidget`을 상속받는 블루프린트 위젯(예: `WBP_ExHUDLayout`)을 생성한 후 다음 단계를 진행합니다.
1. 캔버스 패널이나 오버레이에 `Common Activatable Widget Stack` 두 개를 각각 자식으로 추가합니다.
2. 각 스택의 디테일 패널상 변수 이름(Is Variable)을 다음 구조로 정확히 맞춥니다 (BindWidget 강제).
   - **`GameStack`** (아래쪽 Layer)
   - **`MenuStack`** (위쪽 Layer)
3. **스택 화면 전체 꽉 채우기 (앵커 설정)**
   - 방금 추가한 `GameStack`과 `MenuStack`을 각각 하나씩 클릭합니다. (주의: RootPanel 클릭 X)
   - 우측 디테일(Details) 창 위쪽에 꽃 모양의 **앵커(Anchors)** 버튼을 누르고 화면 모서리 전체가 칠해진 **전체 채우기 (Fill)** 아이콘을 선택합니다.
   - 앵커 값 아래에 뜬 오프셋(Left, Top, Right, Bottom) 값을 모두 **0**으로 만들어 화면 크기에 맞게 꽉 차도록 세팅합니다.

## 4. MVVM 뷰모델 연결 (블루프린트 계층)
1. 체력이나 스코어 바(UI Widget)를 엽니다.
2. Window 탭에서 `Model View View Model (MVVM)` 패널을 엽니다.
3. `Viewmodels` 섹션에서 `+ Add ViewModel`을 누르고 방금 만든 **`UExPlayerStatsViewModel`** 클래스를 선택, 인스턴스 옵션을 `Create Instance`로 둡니다.
4. 이제 디자이너 뷰에서 텍스트 블록의 Text 프로퍼티 옆 체인 아이콘을 눌러, 방금 추가한 ViewModel의 `GetCurrentScore()` 또는 프로퍼티와 연결해 주면 모든 준비가 완료됩니다!
