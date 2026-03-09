# ExFrameWork Unified Input Architecture 안내서

본 문서는 하드웨어 입력(키보드/게임패드)과 UI 터치 입력(버튼 클릭)을 단일 파이프라인으로 통합 처리하는 **컴포넌트 기반 입력 아키텍처(Component-Based Unified Input Architecture)**에 대해 설명합니다.

---

## 1. 아키텍처 핵심 개념

이 아키텍처의 가장 큰 목적은 **"입력 신호의 발생지(Hardware vs UI)와 관계없이, 캐릭터 로직은 오직 한 곳에서만 입력을 수신하도록 디커플링(Decoupling)하는 것"**입니다.

*   **하드웨어 입력 (Enhanced Input):** 키보드, 마우스, 게임패드의 물리적 입력.
*   **UI 입력 (MVVM):** HUD 화면에 떠 있는 가상 패드(버튼)의 터치/클릭 입력.

위 두 가지 전혀 다른 성격의 입력 소스를 **C++ 입력 컴포넌트(`UExInputComponentBase` 파생 클래스)** 하나가 중앙에서 모두 흡수하여, 동일한 언어(C++ 델리게이트 브로드캐스트)로 캐릭터 블루프린트에게 전달합니다.

---

## 2. 시스템 계층 구조 (Data Flow)

입력 신호는 최종적으로 캐릭터 블루프린트의 Event 노드로 떨어질 때까지 다음 계층을 거칩니다.

### 2.1 하드웨어 기반 (Enhanced Input) 플로우
1. 사용자가 **스페이스바(Jump)** 를 누름.
2. `UExRunnerInputComponent`가 `InitializeInputBindings`를 통해 자동 바인딩 해둔 **C++ 콜백 함수(`NativeOnJumpAction`)** 가 발동됨.
3. 콜백 함수 내부에서 **`OnJumpRequested.Broadcast()`** 를 호출함.

### 2.2 UI 터치 기반 (MVVM ViewBinding) 플로우
1. 사용자가 스마트폰 화면에서 **점프 위젯 버튼**을 터치함.
2. 버튼의 `On Clicked` 이벤트가 MVVM View Binding을 타고 **`UExRunnerInputViewModel::OnJumpButtonClicked()`** C++ 함수를 즉시 호출함.
3. ViewModel은 현재 플레이어의 폰(Pawn)에 달린 **`UExRunnerInputComponent`**를 찾아냄.
4. ViewModel이 `InputComponent->RequestJumpAction()`을 호출함.
5. `RequestJumpAction()` 내부에서 똑같이 **`OnJumpRequested.Broadcast()`** 를 호출함.

### 2.3 🎯 최종 실행부 (캐릭터 블루프린트)
결과적으로 키보드를 누르든, 화면을 터치하든 상관없이 입력 컴포넌트는 단 1개의 이벤트 노드(**`On Jump Requested`**)만을 발생시킵니다.
*   캐릭터 블루프린트는 하드웨어 노드(`IA_Jump`)나 UI 버튼 이벤트 노드를 직접 그릴 필요가 **전혀 없습니다.**
*   오로지 보유하고 있는 `Runner Input Component`의 **`On Jump Requested`** 디스패처에만 연결해두면 모든 동작이 일관성 있게 수행됩니다.

---

## 3. 실무 작업 프로세스 가이드 (Blueprint 기준)

새로운 입력 액션(예: 공격, 회피 등)이나 새로운 게임 모드(예: 배틀 모드)를 추가할 때 다음과 같은 워크플로우를 따릅니다.

### Step 1. (C++) 입력 액션 선언 및 브로드캐스트 구현
1. `ExCore`의 `UExInputComponentBase`를 상속받는 피처별 컴포넌트(ex. `UExRunnerInputComponent`)를 생성.
2. `UInputAction` 에셋을 받을 `UPROPERTY` 변수 선언.
3. `DECLARE_DYNAMIC_MULTICAST_DELEGATE`로 BP에 전달할 이벤트 시그니처 생성 (ex. `OnAttackRequested`).
4. `InitializeInputBindings()`를 오버라이드하여 Enhanced Input과 C++ 콜백을 묶음.
5. C++ 콜백과 UI Request 함수에서 공통으로 `OnAttackRequested.Broadcast()`를 호출하도록 작성.

### Step 2. (C++) ViewModel 브릿지 작성
1. `UMVVMViewModelBase`를 상속받는 입력 뷰모델(ex. `UExRunnerInputViewModel`) 생성.
2. `OnAttackButtonClicked` 등의 BlueprintCallable 함수 작성.
3. 함수 내부에서 `Pawn`을 찾고, Step 1에서 만든 `InputComponent`를 찾아 `RequestAttackAction()`을 호출.

### Step 3. (BP) 캐릭터 블루프린트에서 사용하기
1. 캐릭터 BP(예: `ExSandboxCharacter_Mover`)를 엽니다.
2. **Components(컴포넌트) 패널**에서 `Add` 버튼을 눌러 C++로 제작한 `Runner Input Component`를 부착합니다.
3. 컴포넌트를 선택 후 **Details(상세) 패널**로 이동합니다.
4. **Data Category** 항목에 만들어 둔 `UInputAction` 에셋들(Jump, Slide 등)을 할당합니다.
5. 우측 하단의 **Events(이벤트)** 항목에서 `On Jump Requested` 등의 초록색 `+` 버튼을 눌러 이벤트 그래프에 노드를 꺼냅니다.
6. 해당 이벤트 선을 뽑아 점프 로직, 애니메이션 로직 등 원하는 실제 동작을 엮어줍니다. **(이제 기존 `IA_Jump` 등의 입력 전용 노드는 삭제해도 됩니다.)**

### Step 4. (BP) 터치/클릭 UI 패드에서 사용하기 (MVVM View Binding)
1. 터치 버튼이 있는 위젯 디자이너(예: `WBP_ExRunnerInputPad`)를 엽니다.
2. 하단의 **Viewmodels 탭**에서 `+ Viewmodel`을 누르고 작성해둔 `ExRunnerInputViewModel`을 추가합니다.
3. ❗**중요:** 해당 뷰모델 클릭 후 우측 Details 패널에서 **`Creation Type`** 을 반드시 **`Create Instance`** 로 변경합니다.
4. 상단의 **Window -> View Bindings 탭**을 엽니다.
5. `+ Add Widget`을 눌러 소스(버튼)와 데스티네이션(뷰모델 함수)을 시각적으로 엮어줍니다.
   *   *(Source)* `Btn_Jump` ➜ `On Clicked` 이벤트 선택
   *   *(Destination)* `ExRunnerInputViewModel` ➜ `OnJumpButtonClicked` 함수 선택
6. 화살표 방향이 `Source ➔ Destination (One Way to Source)` 인지 확인합니다.
7. **이벤트 그래프(Event Graph)에 들어갔던 기존 더러운 Blueprint Event 노드들을 싹 다 지웁니다.**

---

이 아키텍처를 준수하면, 컨트롤러 지원, 모바일 터치 지원, 입력 키 변경(Rebinding) 등 프로젝트가 커지더라도 스파게티성 노드 연결 없이 직관적이고 견고한 프레임워크를 유지할 수 있습니다.
