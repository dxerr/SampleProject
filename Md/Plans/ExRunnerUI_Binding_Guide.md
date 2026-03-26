# UI 연동 가이드: WBP_ExRunnerSpeedBar (MVVM 기반)

주인님의 프로젝트에 구성된 훌륭한 **MVVM(Model-View-ViewModel)** 시스템을 활용하여, 지저분한 블루프린트 노드 수작업 없이 우아하고 깔끔하게 코인과 스프린트 타이머 UI를 연동하는 가이드입니다. 

---

## 1. 사전 준비 (위젯 열기)
1. 언리얼 에디터의 `Content Browser`를 엽니다.
2. `Plugins/GameFeatures/ExRunnerPlay/Content/UI/Parts/` 경로에 있는 `WBP_ExRunnerSpeedBar` 위젯 블루프린트를 엽니다.

## 2. UI 레이아웃 구성 (Designer 탭)
이전 가이드와 동일하게 배치해 줍니다. 이번엔 `Is Variable`을 체크하지 않아도 MVVM 바인딩이 가능합니다!

1. **코인 개수 텍스트**: Palette에서 `Text` 컴포넌트를 드래그해 적당한 곳에 배치합니다 (예: 이름 `Text_CoinCount`).
2. **스프린트 프로그레스 바**: Palette에서 `ProgressBar` 컴포넌트를 하단에 길게 배치합니다 (예: 이름 `ProgressBar_Sprint`).
3. **스프린트 잔여 시간 텍스트**: `ProgressBar` 중앙에 겹치도록 `Text` 컴포넌트를 배치합니다 (예: 이름 `Text_SprintTime`).

*(팁: ProgressBar 크기 조절은 게이지 겉을 Size Box로 감싸시거나 Size X/Y 셋팅을 활용하시면 깔끔합니다!)*

---

## 3. MVVM ViewModels 추가 및 초기화

### 3.1 ViewModels 패널에서 추가
1. 위젯 디자이너 창 상단 메뉴바 (Window)에서 **`Viewmodels`** 창을 켭니다.
2. Viewmodels 패널 좌측 상단의 **+ Add ViewModel** 버튼을 누릅니다.
3. 클래스(Class)를 **`Ex Runner Stats View Model`** 로 선택합니다.
4. 추가된 뷰모델을 클릭하고 우측 디테일 창에서 Creation Type을 **`Create Instance`** (인스턴스 생성) 로 설정합니다. (매우 중요⭐: 이 설정을 해야 위젯이 스스로 뷰모델 객체를 메모리에 생성하므로 "Accessed None" 에러를 피할 수 있습니다.)

### 3.2 블루프린트 Graph에서 초기화 (단 1개의 노드!)
1. 우측 상단의 **Graph** 탭으로 전환합니다.
2. 기존에 만들었던 코인/스프린트 갱신 관련 지저분한 블루프린트 노드들이 남아있다면 모두 삭제합니다.
3. `Event On Activated` (또는 `Event Construct`) 노드를 준비합니다.
4. 왼쪽 My Blueprint 패널 패널 하단의 `Viewmodel` 카테고리에서 진짜 MVVM용 뷰모델 객체 변수를 드래그(Get)하여 꺼냅니다.
5. 변수 핀을 드래그하여 **`Auto Initialize`** 함수 노드를 연결합니다.
6. `Auto Initialize`의 `In Controller` 핀에는 **`Get Owning Player`** 노드를 연결합니다.

> **끝입니다!** 이제 복잡한 이벤트 델리게이트 노드는 일절 필요 없습니다.

---

## 4. View Bindings 속성 매핑 등록

이제 각 위젯 컴포넌트 속성에 ViewModel의 데이터를 1:1로 매핑해 줍니다. 
1. 디자이너 창 상단 메뉴바 (Window)에서 **`View Bindings`** 창을 켭니다.
2. 패널 하단의 **+ Add Widget** 버튼을 눌러 바인딩 목록을 3개 만드시고 컴파일 에러가 나지 않도록 알맞게 연결합니다.

### 바인딩 1: 코인 개수
- **Target**: `Text_CoinCount` 컴포넌트의 `Text` 속성 매핑
- **Source**: `ExRunnerStatsViewModel`의 **`Get Coin Count Text`** (C++단위 변환 함수) 선택

### 바인딩 2: 프로그레스 바 (게이지)
- **Target**: `ProgressBar_Sprint` 컴포넌트의 `Percent` 속성 매핑
- **Source**: `ExRunnerStatsViewModel`의 **`Get Sprint Progress`** (0.0~1.0 게이지 변환 함수) 선택

### 바인딩 3: 잔여 시간 텍스트
- **Target**: `Text_SprintTime` 컴포넌트의 `Text` 속성 매핑
- **Source**: `ExRunnerStatsViewModel`의 **`Get Sprint Remaining Time Text`** (소수점 포맷팅 완료된 텍스트 함수) 선택

### (옵션) 스프린트가 끝났을 때 숨기기 (Visibility)
스프린트가 끝났을 때 게이지를 깔끔하게 숨기고 싶으시다면 다음과 같이 바인딩을 추가해 편하게 구축하세요:
- **Target**: `ProgressBar_Sprint` 의 `Visibility` 속성
- **Source**: `ExRunnerStatsViewModel`의 `SprintRemainingTime` (Float) 속성 설정
- **Conversion** (가운데 아이콘): 화살표를 눌러 `Conversion Function` 적용 후 내부 로직을 "Float 값이 0보다 크면 Visible, 아니면 Hidden" 을 반환하게 지정하시면 완벽합니다.

---

## 5. 마무리
1. 저장 후 라이브 코딩 컴파일(`Ctrl + Alt + F11`)을 거친 뒤 플레이(PIE) 하시면 깔끔한 MVVM 연동 화면을 즐기실 수 있습니다.
2. 코인 획득 시 UI 갱신 버그는 C++ `ExItemEffect_Score` 소스에서 PlayerState 강제 검사 제약 조건을 풀어 완전히 쾌적하게 해결해 두었습니다!
