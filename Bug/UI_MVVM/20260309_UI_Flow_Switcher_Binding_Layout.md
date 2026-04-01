# UI Flow: CommonAnimatedSwitcher 변수 바인딩 및 레이아웃 절대 좌표 설정 오류

- **날짜**: 2026-03-09
- **상태**: ✅ 해결 완료
- **키워드**: `CommonAnimatedSwitcher`, `MVVM`, `View Binding`, `SetActiveWidgetIndex`, `Canvas Panel`, `Overlay`, `Relative Layout`

---

## 증상

1. MVVM 모델 구조에서 `GameState`의 `MatchPhase`가 변경될 때 스위처가 정상적으로 UI를 전환하지 못함. View Binding 시스템의 "Set Active Widget Index" 기능을 사용할 수 없고 뷰모델 객체가 생성/참조되지 않는 현상 발생.
2. 스위처의 하위 엘리먼트로 Overlay를 사용하여 하위 위젯들을 삽입했을 때, Anchor 좌표값과 Size 지정 옵션이 사라지고 Relative Layout으로만 처리되어 화면 구석으로 몰리거나 크기 조절/배치가 불가능해짐.

---

## 원인 분석 요약

### 문제 1: CommonAnimatedSwitcher는 MVVM Property Setter를 제공하지 않음
- **원인**: 언리얼 엔진 5의 MVVM 플러그인과 `CommonAnimatedSwitcher`의 호환성 문제. 런타임에 Setter 프로퍼티로 쓸 수 있는 C++ `UPROPERTY`가 누락되어 직접적인 View Binding 갱신 불가. 이로 인해 에디터에서 View Binding을 끊어버리면 엔진 레벨의 최적화로 `Event On Activated` 시 활성화 여부와 무관하게 뷰모델 인스턴스 자체가 초기화되지 않아(Null) 로그조차 찍히지 않음.
- **해결**: C++의 `UExRunnerMatchViewModel`에 `BindSwitcher(UCommonAnimatedSwitcher*)` 함수를 새로 만들어 BP상에서 뷰모델과 대상 위젯(Switcher)을 다이렉트 포인터로 강제 결합시킴. 페이즈 변동 시 ViewModel 내부에서 `BoundSwitcher->SetActiveWidgetIndex()`를 직접 통보하게 우회.

### 문제 2: 스위처 하위 부모 패널 컨테이너의 (Absolute vs Relative) 구조상 한계
- **원인**: 스위처의 Page Index를 구분하기 위해 각 화면 단위로 `Overlay`를 첫 번째 부모로 두었음. Overlay, Horizontal/Vertical 박스는 "상대 좌표(Relative Layout)"로 종속된 위젯들을 다루다 보니 픽셀 단위로 위치와 사이즈, Anchor 지정을 하는 옵션(Size X/Y, Position X/Y)이 노출 불가.
- **해결**: 스위처의 0, 1, 2 페이지 하단에 `Canvas Panel`을 중간 부모 구조로 끼워 넣음. Canvas Panel은 절대 좌표(Absolute Positioning)를 지원하므로 기존 루트 Canvas에 두었을 때와 100% 동일하게 Anchor를 기준으로 자유로운 배치가 가능해짐.

---

## 핵심 교훈

1. **에디터 한계의 직관화**: 에디터 상단부 MVVM View Bindings에 의존하기 어려운 Property 대상은, 억지로 이벤트를 돌리기보다는 **ViewModel C++ 코드를 확장하여 위젯 포인터를 쥐어주는** 우회 결합 패턴이 더 확실하다.
2. **패널(Panel) 레이아웃 속성 숙지**: UMG에서 자유로운 픽셀 배치와 Anchor 활용이 필요한 컴포넌트 묶음은 반드시 `Canvas Panel`로 감싸서 관리할 것. `Overlay` 등은 화면 크기가 아니라 Content 영역만큼만 자리를 차지하는 반응형 구조임에 유의.
