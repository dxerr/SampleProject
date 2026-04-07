# ExCore Common Popup & Toast UI 셋업 가이드

> 최종 갱신: 2026-04-07

이 문서는 `ExCore`의 데이터 드리븐(Data-Driven) 공통 팝업 및 토스트 UI 시스템을 프로젝트에 도입하고 세팅하는 과정을 가장 직관적이고 심플하게 정리한 퀵 가이드입니다.

---

## 1. 기반 위젯(WBP) 만들기

### 1-1. 공용 버튼 위젯 (`WBP_BaseButton`)
일반적인 언리얼 UMG 버튼(기본 UButton)과는 다릅니다. 이 시스템은 **Common UI**를 기반으로 작동합니다.

1. 위젯 블루프린트 생성 후 부모 클래스를 **`ExBaseButtonWidget`**으로 상속합니다.
2. 내부에 절대로 기본 UButton 위젯을 추가하지 마세요. (위젯의 Root 자체가 버튼 역할을 합니다.)
3. `Overlay`를 깔고 배경(Border 등)과 글자(`Common Text` 등)를 디자인합니다.
4. 글자가 표시될 텍스트 위젯의 이름을 **`ButtonTextBlock`**으로 짓고 `Is Variable`에 체크합니다. (이렇게 해야 팝업이 버튼 글자를 주입할 수 있습니다.)

### 1-2. 팝업 위젯 (`WBP_ExPopup`)
1. 위젯 블루프린트를 만들고 부모를 **`ExPopupWidget`**으로 상속합니다.
2. 컴포넌트 창 최상단(Root)을 클릭 후, 우측 디테일 창 **[ExUI \| Popup]** 카테고리에서 **`Popup Button Class`** 값을 방금 만든 **`WBP_BaseButton`**으로 설정합니다.
3. 배경 Border 카드가 동그랗게 찌그러지면 우측 설정에서 `Rounding Type`을 `Half Height Radius` 대신 **`Fixed Radius`**로 바꿉니다.
4. 동적 버튼들이 생성될 그릇인 `Horizontal Box`를 만들고 이름을 **`Panel_Buttons`** (Is Variable) 로 짓습니다.
   * 버튼 그룹 전체가 가운데로 오게 하려면, `Panel_Buttons`의 부모 슬롯(Slot) 설정에서 **Horizontal Alignment (수평 정렬)**을 **Center(가운데 정렬 ◫)** 모양으로 변경합니다!
5. 텍스트 입력창이 필요하다면 반드시 **`Text Box`** (Editable Text 아님주의) 클래스를 배치하고 이름을 **`EditBox_Input`** (Is Variable) 으로 설정합니다.

### 1-3. 토스트 위젯 (`WBP_ExToast`)
1. 위젯 블루프린트를 만들고 부모를 **`ExToastWidget`**으로 상속합니다.
2. 문구를 띄워줄 `Common Text`를 배치하고 이름을 **`Text_Message`** 로 설정합니다.
3. **(선택 사항) 애니메이션 연동**:
   * 블루프린트 이벤트 그래프에서 `Event Play Intro Animation` 파란색 핀을 꺼내 트윈(Tween) 애니메이션을 재생할 수 있습니다.
   * `Event Play Outro Animation`에서는 퇴장 연출을 재생하고, 애니메이션이 끝날 때 **반드시 `Finish Close Toast` 함수**를 노드로 끝에 추가해야 완전히 메모리에서 해제됩니다.
   * _(참고: 애니메이션 노드를 연결하지 않고 비워두면 버그 방지를 위해 즉시 자동으로 파괴되도록 C++ 쪽에 기본 구현(Fallback)이 되어있습니다.)_

---

## 2. 메인 HUD에 띄우기 (연동 셋업)

팝업과 토스트는 공중에 아무 데나 뜰 수 없고, 도화지가 될 **Main HUD** 위젯이 반드시 필요합니다.

### 2-1. HUD 디자이너 화면 준비
1. 팝업창을 띄울 **`Common Activatable Widget Stack`** 위젯을 추가하고 이름은 `MenuStack` 같이 지어줍니다.
2. 토스트들이 등장할 구역(보통 우측 하단이나 상단 중앙)에 **`Vertical Box(수직 박스)`** 를 추가합니다.
3. 수직 박스의 이름을 반드시 **`ToastContainer`** 로 짓고 우측 상단의 `Is Variable (변수인지)` 체크를 켭니다.

### 2-2. HUD 이벤트 그래프 연동
 HUD 위젯의 `Event Construct` 쪽에 다음과 같은 노드 연결(세팅) 흐름을 만들어줍니다.
1. `Get Local Player Subsystem` -> 클래스: **`ExUIManagerSubsystem`**
2. **`Set Popup Widget Class`**: 여기에 위의 1-2번에서 만든 `WBP_ExPopup` 지정
3. **`Set Toast Widget Class`**: 여기에 위의 1-3번에서 만든 `WBP_ExToast` 지정
4. **`Register Stacks`**: HUD 화면 구석구석에 만들어둔 `GameStack`, `MenuStack` 등 연동 (팝업 사용처 지정)
5. **`Register Toast Container`**: HUD의 `ToastContainer` 변수를 당겨와서 연결!

---

## 3. 손쉬운 개발자 테스트 (치트 확장 기능)

UI 연동이 잘 되었는지 즉각 확인하고 싶을 땐 C++ 네이티브 `ExCoreCheatExtension` 기능을 사용하시면 가장 편합니다.

### 3-1. ExCore 플러그인지 등록
1. 에디터 밖에서 C++ 코드가 바뀌었다면 가급적 언리얼 에디터를 껐다가 **라이브 코딩이 아닌 아예 재실행**해야 엔진 리플렉션(새로운 명령어)이 모두 업데이트됩니다.
2. 콘텐츠 브라우저에서 `ExCore/GameFeatureData` 에셋을 엽니다.
3. `Actions` 내부에 있는 **`Add Cheats`** 배열 목록에 **`ExCoreCheatExtension`** (또는 BP_ExCoreCheatExtension)이 정상적으로 들어가 있는지 확인합니다.

### 3-2. 콘솔(`~`) 커맨드 치트 테스트
플레이 버튼을 누르고 백틱(`~`) 키를 눌러 다음 명령어 파라미터들을 입력해 봅니다.

| 치트 커맨드 / 예제 | 설명 |
|---|---|
| `ExUITestInfo [Title] [Body]` <br><br> 예) `ExUITestInfo "서버" "5분뒤 닫힘"` | **정보 팝업 테스트**. 버튼 없이 표시되었다가 3초 뒤 자동 폭파됩니다. |
| `ExUITestConfirm [Title] [Body]` <br><br> 예) `ExUITestConfirm "장비 파괴" "정말 파괴?"` | **버튼 팝업(Confirm) 테스트**. 확인/취소 버튼이 동적으로 뜹니다.<br>버튼을 누를 시 로그 파일에 결과값(숫자)이 출력됩니다! |
| `ExUITestToast [Message]` <br><br> 예) `ExUITestToast "네트워크 끊김"` | **토스트 알림 테스트**. 앞서 만든 `ToastContainer` 내부에 줄줄이 쌓였다가 3초 뒤 사라집니다. |

---

## 4. 응용 편: 버튼을 마음대로 늘리고 싶어요!

치트에서 제공하는 것은 기본 설정이지만, 실제 인게임 로직을 짤 때 블루프린트에서 무한개의 버튼을 가진 팝업창을 띄우려면 다음과 같이 하시면 됩니다.

1. 블루프린트에서 서브시스템의 **`Show Popup BP (Advanced)`** 노드를 가져옵니다.
2. `Descriptor` 점선 핀을 뽑아서 **`Make ExPopupDescriptor`** 노드를 꺼냅니다.
3. 그 노드 옵션 중에 **`Buttons`** (버튼 목록) 배열이 보입니다.
4. '+' 버튼을 눌러 배열을 원하는 만큼 추가하고 요소마다 `Label`(표시될 글자 텍스트)과 `Result Value`(선택 시 반환될 결과 Enum 지정)를 세팅하시면 **C++ 시스템이 해당 개수에 맞춰 자동으로 버튼을 생성하여 가로 중앙에 예쁘게 배열**해 줍니다!
