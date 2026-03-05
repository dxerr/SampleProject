# ExFrameWork: 모던 UI 시스템 아키텍처 설계서

> **버전:** v2.0  
> **대상 엔진:** Unreal Engine 5  
> **프로젝트:** ExFrameWork  
> **작성일:** 2026-03-04  
> **의존 문서:** ExFrameWork_Multiplayer_Flow_Architecture.md (v2.0)

---

## 1. 아키텍처 개요 및 핵심 원칙

### 1.1 목표
확장성이 뛰어나고 성능이 최적화된 UI 시스템을 구축한다. 이 시스템은 다음의 UE5 최신 UI 패러다임을 준수한다:

- **Common UI 플러그인:** 입력 라우팅(Input Routing), 레이어 스택 관리, 크로스 플랫폼 컨트롤러 지원.
- **MVVM (Model-View-ViewModel):** 이벤트 기반의 데이터 바인딩. UI 업데이트를 위해 `Event Tick`을 절대 사용하지 않는다.
- **하이브리드 접근법 (C++ 베이스 / BP 디자인):** 핵심 로직과 변수는 C++ `UPROPERTY(meta = (BindWidget))`으로 정의한다. 시각적 요소와 애니메이션은 UMG 블루프린트에서 처리한다.
- **UI 서브시스템:** 흩어진 `CreateWidget` 호출을 방지하고, UI Push/Pop을 중앙에서 처리하는 매니저 클래스.

### 1.2 Flow 아키텍처와의 관계

이 UI 시스템은 `ExFrameWork_Multiplayer_Flow_Architecture.md (v2.0)`에서 정의한 레이어 구조를 따른다. 모든 UI 베이스 클래스는 **ExCore 플러그인**에 배치한다 (ExCore는 항상 활성화 전제). 게임 피처별 확장 UI는 해당 GameFeature 플러그인에 배치한다.

### 1.3 UI 데이터 흐름 (멀티플레이어 환경)

UI는 서버 데이터를 직접 참조하지 않는다. 반드시 아래 경로를 따른다:

```
서버(GameMode) → 복제(Replicate) → GameState/PlayerState → OnRep → 로컬 델리게이트
→ ViewModel(FieldNotify) → Widget 자동 업데이트

예시 1: 매치 상태 변경
  GameMode::SetMatchPhase() 
    → GameState::CurrentMatchPhase (복제)
      → OnRep_MatchPhase() 
        → OnMatchPhaseChanged 델리게이트 Broadcast
          → UExMatchViewModel::SetMatchPhase()
            → FieldNotify → HUD Widget 자동 업데이트

예시 2: 점수 변경
  GameMode에서 PlayerState::AddScore() 호출
    → PlayerState::CurrentScore (복제)
      → OnRep_Score()
        → OnScoreChanged 델리게이트 Broadcast
          → UExScoreViewModel::SetScore()
            → FieldNotify → 스코어보드 Widget 자동 업데이트
```

**핵심 규칙:** 서버가 위젯을 생성하거나 조작해서는 절대 안 된다. UI는 복제된 데이터의 변경 알림에 의해서만 수동적으로 구동된다.

### 1.4 레이어 배치 규칙

| 클래스 | 배치 위치 | 이유 |
|---|---|---|
| `UExUIManagerSubsystem` | **ExCore** | 모든 게임 모드에서 공용으로 사용하는 UI 스택 관리자 |
| `UExHUDLayoutWidget` | **ExCore** | HUD 골격 베이스 (GameStack/MenuStack 컨테이너) |
| `UExWindowWidget` | **ExCore** | 창모드형 위젯 베이스 |
| `UExModalWidget` | **ExCore** | 팝업형(Modal/Modeless) 위젯 베이스 |
| `UExBaseButtonWidget` | **ExCore** | 공용 버튼 스타일 베이스 |
| ViewModel 베이스 | **ExCore** | MVVM 기반 클래스 |
| `UExRunnerHUDLayout` | **ExRunnerPlay** | 러너 전용 HUD Layout 확장 |
| `UExRunnerScoreWidget` | **ExRunnerPlay** | 러너 전용 스코어 UI |

---

## 2. 엄격한 제약 사항 (모든 단계에 적용)

### 2.1 CommonUI 필수 규칙

**입력 모드 관리:**
- `SetInputModeGameOnly()`, `SetInputModeUIOnly()`, `SetInputModeGameAndUI()` 를 **절대 사용하지 마라**. CommonUI 환경에서 이 함수들은 입력 라우팅 시스템을 완전히 깨뜨린다.
- 대신 각 `UCommonActivatableWidget` 파생 클래스에서 `GetDesiredInputConfig()` 함수를 오버라이드하여 해당 위젯이 활성화될 때 적용할 입력 설정을 **선언적으로** 반환한다.

**위젯 활성화:**
- `ActivateWidget()`을 수동으로 호출하지 마라. `UCommonActivatableWidgetContainerBase::AddWidget()`(= `PushWidget`)이 위젯을 스택에 추가하면 자동으로 활성화된다. 수동 호출은 이중 활성화를 유발하여 입력 라우팅이 망가진다.

**GameViewportClient:**
- 프로젝트 설정에서 `GameViewportClientClass`를 반드시 `CommonGameViewportClient`로 변경해야 한다. 이것을 빼먹으면 CommonUI의 입력 라우팅 시스템이 전혀 동작하지 않으며, 에디터 로그에 `"Using CommonUI without a CommonGameViewportClient"` 에러가 출력된다.

**위젯 베이스 클래스:**
- 메뉴, 창, 팝업의 베이스 클래스로 `UUserWidget`을 절대 사용하지 마라. 반드시 `UCommonActivatableWidget` 또는 그 파생 클래스를 사용한다.
- 단, 상호작용이 필요 없거나 입력 라우팅에 영향을 주지 않는 단순 표시 위젯(툴팁, 데미지 숫자 등)은 `UCommonUserWidget` 또는 `UUserWidget`을 사용할 수 있다.

### 2.2 MVVM 필수 규칙

- UI 업데이트를 위해 `Event Tick`을 절대 사용하지 마라. FieldNotify 기반 데이터 바인딩만 사용한다.
- ViewModel 프로퍼티는 `BlueprintReadOnly, FieldNotify`로 선언한다. (`BlueprintReadWrite`가 아님. ReadWrite로 하면 블루프린트에서 Setter를 거치지 않고 직접 변경하여 Broadcast가 누락된다.)
- 값 변경은 반드시 Setter 함수를 통해 수행하며, Setter 내부에서 `UE_MVVM_SET_PROPERTY_VALUE` 매크로를 호출한다.

### 2.3 코딩 규칙

- 하드코딩된 UI 텍스트에 `FText::FromString()`을 무분별하게 사용하지 마라. 현지화(Localization)를 대비하여 `NSLOCTEXT` 또는 `LOCTEXT` 매크로 사용을 기본으로 한다.
- `NativeConstruct()`에서 델리게이트를 바인딩하기 전에 반드시 포인터 유효성 검사(`if (WidgetPtr)`)를 수행한다.
- 코드는 명확하게 작성하고 주석을 충분히 단다. 에디터에서 사용자가 직접 수행해야 하는 작업은 **"수동 에디터 단계(Manual Editor Step)"**라는 명칭으로 명확하게 안내한다.
- `CommonButtonBase` 내부에 `CommonActionWidget`을 배치할 때, 이름을 정확히 **`InputActionWidget`**으로 지정해야 게임패드 아이콘이 자동 표시된다. 다른 이름을 사용하면 표시되지 않는다.

---

## 3. 위젯 3분류 체계 정의

이 프로젝트는 UI 위젯을 용도에 따라 3가지 베이스 클래스로 분류한다. 각 분류는 CommonUI의 입력 라우팅 요구사항이 명확히 다르므로 별도의 베이스 클래스로 분리한다.

### 3.1 분류 요약

| 분류 | 베이스 클래스명 | 용도 | 스택 관계 | 입력 모드 |
|---|---|---|---|---|
| HUD Layout | `UExHUDLayoutWidget` | 항상 화면에 존재하는 게임 HUD 골격 | 스택의 **소유자** (Push/Pop 대상 아님) | Game (마우스 캡처, 게임 입력 활성) |
| 창모드형 (Window) | `UExWindowWidget` | 인벤토리, 설정, 캐릭터 정보 등 대형 패널 | GameStack 또는 MenuStack에 **Push되는 대상** | Menu (마우스 표시, 게임 입력 비활성/부분) |
| 팝업형 (Modal/Modeless) | `UExModalWidget` | 확인 대화상자, 경고, 보상 팝업 등 소형 오버레이 | MenuStack에 **Push되어 최상단 차지** | Modal: Menu전용 / Modeless: All |

### 3.2 스택 구조 다이어그램

```
UExHUDLayoutWidget (항상 활성, Viewport에 직접 추가)
│
├── GameStack (UCommonActivatableWidgetStack)
│   │  [Z-Order: 하위]
│   ├── 미니맵 오버레이
│   ├── 퀘스트 트래커
│   └── 게임 중 알림 등
│
└── MenuStack (UCommonActivatableWidgetStack)
    │  [Z-Order: 상위, GameStack보다 위에 렌더링]
    │
    ├── UExWindowWidget 파생 (인벤토리, 설정 등)     ← Push/Pop
    │   └── 탭 전환, 목록 등 복잡한 내부 구조 가능
    │
    └── UExModalWidget 파생 (확인창, 보상 팝업 등)    ← Push/Pop (최상단)
        └── Modal은 입력 독점, Modeless는 입력 통과
```

**CommonUI 입력 라우팅 동작:**
- CommonUI는 최상단에 렌더링된 Activatable Widget에게 입력을 라우팅한다.
- MenuStack에 위젯이 Push되면, 해당 위젯이 입력을 가져간다.
- MenuStack이 비어 있으면 GameStack의 활성 위젯이 입력을 받는다.
- GameStack도 비어 있으면 HUD Layout이 입력을 받는다.

---

## 4. 1단계 구현: 환경 및 모듈 설정

> **실행 지시:** 이 단계를 가장 먼저 수행한다. 코드 작성 전에 프로젝트 환경을 반드시 설정해야 한다.

### 4.1 Build.cs 모듈 추가

**에이전트 작업:** ExCore 플러그인의 `.Build.cs` 파일에 다음 모듈을 추가한다.

```csharp
// ExCore.Build.cs
PublicDependencyModuleNames.AddRange(new string[]
{
    "UMG",
    "CommonUI",
    "CommonInput",        // ← 원본 문서에서 누락. CommonUI 입력 시스템에 필수
    "ModelViewViewModel",
});
```

### 4.2 수동 에디터 단계 (Manual Editor Steps)

아래 항목들은 C++ 코드가 아닌, 언리얼 에디터에서 사용자가 직접 설정해야 하는 작업이다. **이 설정 없이는 CommonUI가 정상 동작하지 않으므로, 코드 구현 전에 반드시 완료해야 한다.**

#### 4.2.1 플러그인 활성화
1. 에디터 메뉴 `Edit → Plugins`로 이동한다.
2. **Common UI Plugin**을 검색하여 활성화한다.
3. **UI Model View View Model** 플러그인을 검색하여 활성화한다.
4. 에디터를 재시작한다.

#### 4.2.2 GameViewportClientClass 변경 (필수)
1. `Edit → Project Settings → Engine → General Settings`로 이동한다.
2. `Game Viewport Client Class`를 **`CommonGameViewportClient`**로 변경한다.
3. 에디터를 재시작한다.

**이 설정을 빼먹으면:** CommonUI의 입력 라우팅 시스템이 전혀 동작하지 않는다. 로그에 `"Using CommonUI without a CommonGameViewportClient derived game viewport client. CommonUI Input routing will not function correctly."` 에러가 출력된다.

#### 4.2.3 Enhanced Input 연동 활성화
1. `Edit → Project Settings → Game → Common Input Settings`로 이동한다.
2. **`Enable Enhanced Input Support`** 체크박스를 활성화한다.

#### 4.2.4 CommonUI 입력 데이터 에셋 생성
아래 에셋들은 CommonUI의 입력 시스템이 동작하기 위해 필요하다.

**1) Input Action Data Table 생성:**
1. 콘텐츠 브라우저에서 `우클릭 → Miscellaneous → Data Table`을 선택한다.
2. Row Structure로 `CommonInputActionDataBase`를 선택한다.
3. 이름: `DT_CommonInputActions`
4. 최소 다음 행(Row)을 추가한다:
   - `Confirm` — 키보드: Enter, 게임패드: Face Button Bottom (A/Cross)
   - `Cancel` (= Back) — 키보드: Escape, 게임패드: Face Button Right (B/Circle)
   - `TabLeft` — 키보드: Q, 게임패드: Left Shoulder
   - `TabRight` — 키보드: E (또는 R), 게임패드: Right Shoulder

**2) CommonUIInputData 블루프린트 생성:**
1. 콘텐츠 브라우저에서 `우클릭 → Blueprint Class → CommonUIInputData`를 검색하여 생성한다.
2. 이름: `BP_CommonUIInputData`
3. 블루프린트를 열고 `Default Click Action`과 `Default Back Action`을 위 Data Table의 항목으로 설정한다.
4. `Edit → Project Settings → Game → Common Input Settings → Input Data`에 이 에셋(`BP_CommonUIInputData`)을 지정한다.

**3) Controller Data Asset 생성:**
1. 콘텐츠 브라우저에서 `우클릭 → Blueprint Class → CommonInputBaseControllerData`를 검색하여 2개 생성:
   - `BP_ControllerData_MKB` (Mouse & Keyboard용)
   - `BP_ControllerData_Gamepad` (Gamepad용)
2. 각 에셋 내부에서 키/버튼에 대한 아이콘 이미지를 설정한다.
3. `Edit → Project Settings → Game → Common Input Settings → Platform Input`에서:
   - Windows 플랫폼에 위 두 Controller Data를 연결한다.
   - `Default Gamepad Name`을 `"Generic"`으로 변경한다 (기본 "Windows"에서 변경 필요).

### 4.3 검증 체크리스트
- [ ] 에디터 `Output Log`에 CommonUI 관련 에러가 없다.
- [ ] PIE(Play In Editor) 시작 시 `"Using CommonUI without a CommonGameViewportClient"` 에러가 출력되지 않는다.
- [ ] ExCore의 Build.cs에 `UMG`, `CommonUI`, `CommonInput`, `ModelViewViewModel` 모듈이 추가되어 있다.
- [ ] `BP_CommonUIInputData`가 프로젝트 설정의 Input Data에 할당되어 있다.

---

## 5. 2단계 구현: UI 매니저 서브시스템

> **실행 지시:** 1단계(환경 설정) 완료 후 이 단계를 구현한다. 사용자의 승인을 받은 후 3단계로 진행한다.

### 5.1 클래스 정보

| 항목 | 값 |
|---|---|
| 클래스명 | `UExUIManagerSubsystem` |
| 부모 클래스 | `ULocalPlayerSubsystem` |
| 배치 위치 | `Plugins/GameFeatures/ExCore/Source/ExCoreRuntime/UI/Subsystems/` |
| 모듈 | ExCore |

### 5.2 ULocalPlayerSubsystem을 선택한 이유

`UGameInstanceSubsystem`이 아닌 `ULocalPlayerSubsystem`을 사용하는 이유:
- 화면 분할(Split Screen) 시 각 로컬 플레이어가 독립적인 UI 스택을 가져야 한다.
- `ULocalPlayerSubsystem`은 각 `ULocalPlayer`에 종속되므로 플레이어별 UI 격리가 자동으로 보장된다.
- UI 코드에서 서브시스템에 접근할 때: `GetOwningLocalPlayer()->GetSubsystem<UExUIManagerSubsystem>()` 패턴을 사용한다.

### 5.3 핵심 요구사항

#### 5.3.1 위젯 스택 관리

이 서브시스템은 두 개의 `UCommonActivatableWidgetStack`에 대한 참조를 보유한다. 스택 자체는 `UExHUDLayoutWidget` (3단계에서 구현)이 생성하고, 서브시스템에 등록한다.

```cpp
// 스택 참조 (HUD Layout이 생성 후 등록)
UPROPERTY()
TObjectPtr<UCommonActivatableWidgetStack> GameStack;

UPROPERTY()
TObjectPtr<UCommonActivatableWidgetStack> MenuStack;
```

#### 5.3.2 공개 인터페이스

```cpp
// 스택 등록 (HUD Layout에서 호출)
UFUNCTION(BlueprintCallable, Category="ExUI")
void RegisterStacks(UCommonActivatableWidgetStack* InGameStack, UCommonActivatableWidgetStack* InMenuStack);

// 창모드형 위젯을 MenuStack에 Push
UFUNCTION(BlueprintCallable, Category="ExUI")
UCommonActivatableWidget* PushWindow(TSubclassOf<UExWindowWidget> WidgetClass);

// Modal/Modeless 위젯을 MenuStack에 Push
UFUNCTION(BlueprintCallable, Category="ExUI")
UCommonActivatableWidget* PushModal(TSubclassOf<UExModalWidget> WidgetClass);

// 게임 오버레이를 GameStack에 Push
UFUNCTION(BlueprintCallable, Category="ExUI")
UCommonActivatableWidget* PushGameOverlay(TSubclassOf<UCommonActivatableWidget> WidgetClass);

// MenuStack 최상단 위젯 Pop
UFUNCTION(BlueprintCallable, Category="ExUI")
void PopMenu();

// 특정 위젯 제거
UFUNCTION(BlueprintCallable, Category="ExUI")
void RemoveWidget(UCommonActivatableWidget* Widget);
```

#### 5.3.3 구현 시 주의사항

**PushWindow/PushModal 내부 구현:**
```cpp
UCommonActivatableWidget* UExUIManagerSubsystem::PushWindow(TSubclassOf<UExWindowWidget> WidgetClass)
{
    if (!MenuStack)
    {
        UE_LOG(LogExUI, Warning, TEXT("PushWindow 실패: MenuStack이 등록되지 않음"));
        return nullptr;
    }
    // AddWidget은 위젯을 생성하고 자동으로 ActivateWidget()을 호출한다.
    // 수동으로 ActivateWidget()을 호출하지 마라.
    return MenuStack->AddWidget(WidgetClass);
}
```

**절대 금지:**
- 이 서브시스템에서 `SetInputMode` 계열 함수를 호출하지 마라. 입력 모드는 각 위젯의 `GetDesiredInputConfig()` 오버라이드가 처리한다.
- `AddWidget` 후에 `ActivateWidget()`을 수동 호출하지 마라. 이중 활성화가 발생한다.

#### 5.3.4 포함해야 할 헤더
```cpp
#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Widgets/CommonActivatableWidgetContainerBase.h"
#include "ExUIManagerSubsystem.generated.h"
```

### 5.4 검증 체크리스트
- [ ] `PushWindow()`를 호출하면 위젯이 MenuStack에 추가되고 자동 활성화된다.
- [ ] `PopMenu()`를 호출하면 최상단 위젯이 제거되고, 아래 위젯이 자동으로 입력을 받는다.
- [ ] MenuStack이 비어 있을 때 게임 입력이 정상적으로 HUD Layout을 통해 게임에 전달된다.
- [ ] `SetInputMode` 계열 함수가 코드 어디에도 존재하지 않는다.
- [ ] `RegisterStacks`가 호출되기 전에 `PushWindow`를 호출하면 경고 로그가 출력되고 `nullptr`이 반환된다.

---

## 6. 3단계 구현: 3분류 베이스 위젯 클래스

> **실행 지시:** 2단계 승인 후 이 단계를 구현한다. 사용자의 승인을 받은 후 4단계로 진행한다.

### 6.1 공통 사항

3개의 베이스 클래스 모두 `UCommonActivatableWidget`을 상속한다. 각 베이스는 다음을 공통으로 제공한다:

- `GetDesiredInputConfig()` 오버라이드 (각 분류에 맞는 기본 입력 설정)
- `NativeOnActivated()` / `NativeOnDeactivated()` 오버라이드
- 블루프린트 디자이너가 시각적 전환 효과를 구현할 수 있는 `BlueprintImplementableEvent`
- `GetDesiredFocusTarget()` 오버라이드 (게임패드 포커스 기본 대상 지정)

### 6.2 ① UExHUDLayoutWidget — HUD Layout용

| 항목 | 값 |
|---|---|
| 클래스명 | `UExHUDLayoutWidget` |
| 부모 클래스 | `UCommonActivatableWidget` |
| 배치 위치 | `Plugins/GameFeatures/ExCore/Source/ExCoreRuntime/UI/Widgets/` |

#### 6.2.1 역할
- 게임 플레이 중 항상 화면에 존재하는 HUD의 뼈대.
- 내부에 `GameStack`과 `MenuStack` (둘 다 `UCommonActivatableWidgetStack`)을 보유한다.
- Push/Pop 스택의 **대상이 아니다**. Viewport에 직접 추가되는 **루트 위젯**이다.
- ESC 키(또는 게임패드 Start 버튼) 입력 시 MenuStack에 일시정지 메뉴를 Push하는 진입점 역할.

#### 6.2.2 핵심 구현

```cpp
UCLASS(Abstract, BlueprintType, Blueprintable)
class EXCORERUNTIME_API UExHUDLayoutWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    // HUD Layout이 활성화될 때의 입력 설정: 게임 입력 활성, 마우스 캡처
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;

    // BP에서 시각적 초기화/정리 수행
    UFUNCTION(BlueprintImplementableEvent, Category="ExUI", meta=(DisplayName="On HUD Activated"))
    void BP_OnHUDActivated();

    UFUNCTION(BlueprintImplementableEvent, Category="ExUI", meta=(DisplayName="On HUD Deactivated"))
    void BP_OnHUDDeactivated();

    // BindWidget: BP에서 반드시 같은 이름으로 위젯을 배치해야 함
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCommonActivatableWidgetStack> GameStack;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCommonActivatableWidgetStack> MenuStack;

    // ESC 메뉴로 Push할 위젯 클래스 (에디터에서 BP 설정)
    UPROPERTY(EditDefaultsOnly, Category="ExUI")
    TSubclassOf<UExWindowWidget> EscapeMenuClass;

private:
    // NativeOnActivated에서 UIManagerSubsystem에 스택 등록
    void RegisterStacksToManager();
};
```

#### 6.2.3 GetDesiredInputConfig 구현
```cpp
TOptional<FUIInputConfig> UExHUDLayoutWidget::GetDesiredInputConfig() const
{
    // HUD Layout: 게임 입력 활성, 마우스 캡처
    // ECommonInputMode::Game = 게임 입력만 받음 (UI 네비게이션 비활성)
    return FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently, /*bHideCursor=*/ true);
}
```

#### 6.2.4 NativeOnActivated 구현
```cpp
void UExHUDLayoutWidget::NativeOnActivated()
{
    Super::NativeOnActivated();
    RegisterStacksToManager();
    BP_OnHUDActivated();
}

void UExHUDLayoutWidget::RegisterStacksToManager()
{
    if (ULocalPlayer* LP = GetOwningLocalPlayer())
    {
        if (UExUIManagerSubsystem* UIManager = LP->GetSubsystem<UExUIManagerSubsystem>())
        {
            UIManager->RegisterStacks(GameStack, MenuStack);
        }
    }
}
```

#### 6.2.5 하위 클래스 확장 예시
```
UExHUDLayoutWidget (ExCore 베이스)
  └── UExRunnerHUDLayout (ExRunnerPlay)
        - 러너 전용 UI 요소 추가 (속도 표시, 거리 카운터 등)
        - EscapeMenuClass에 러너 전용 일시정지 메뉴 지정
```

#### 6.2.6 포함해야 할 헤더
```cpp
#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Widgets/CommonActivatableWidgetContainerBase.h"
#include "ExHUDLayoutWidget.generated.h"

// 전방 선언
class UExUIManagerSubsystem;
class UExWindowWidget;
```

---

### 6.3 ② UExWindowWidget — 창모드형 (Window) Layout용

| 항목 | 값 |
|---|---|
| 클래스명 | `UExWindowWidget` |
| 부모 클래스 | `UCommonActivatableWidget` |
| 배치 위치 | `Plugins/GameFeatures/ExCore/Source/ExCoreRuntime/UI/Widgets/` |

#### 6.3.1 역할
- 인벤토리, 설정, 캐릭터 정보, 상점 등 **전체 화면 또는 대형 패널** 형태의 UI.
- `UIManagerSubsystem::PushWindow()`를 통해 **MenuStack에 Push되는 대상**.
- 활성화 시 게임 입력을 비활성화하고 마우스 커서를 표시한다.
- Back 입력(ESC/게임패드 B 버튼) 시 자동으로 스택에서 Pop된다 (CommonUI 기본 동작).
- 탭(Tab) 전환이 필요한 경우 `UCommonTabListWidgetBase` 통합을 지원한다.

#### 6.3.2 핵심 구현

```cpp
UCLASS(Abstract, BlueprintType, Blueprintable)
class EXCORERUNTIME_API UExWindowWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    // 창 위젯: 메뉴 입력 모드, 마우스 커서 표시
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

    // 게임패드 사용 시 기본 포커스 대상
    virtual UWidget* NativeGetDesiredFocusTarget() const override;

protected:
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;

    // BP 전환 효과 이벤트
    UFUNCTION(BlueprintImplementableEvent, Category="ExUI", meta=(DisplayName="On Window Opened"))
    void BP_OnWindowOpened();

    UFUNCTION(BlueprintImplementableEvent, Category="ExUI", meta=(DisplayName="On Window Closed"))
    void BP_OnWindowClosed();

    // 게임패드 기본 포커스 대상 (BP에서 설정)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ExUI|Focus")
    FName DefaultFocusWidgetName;

    // 이 창이 배경 게임 입력도 허용할지 여부
    // true면 ECommonInputMode::All, false면 ECommonInputMode::Menu
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ExUI|Input")
    bool bAllowGameInputWhileOpen = false;
};
```

#### 6.3.3 GetDesiredInputConfig 구현
```cpp
TOptional<FUIInputConfig> UExWindowWidget::GetDesiredInputConfig() const
{
    if (bAllowGameInputWhileOpen)
    {
        // 게임+UI 모두 입력 가능 (예: 인게임 미니맵 확대)
        return FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::NoCapture, /*bHideCursor=*/ false);
    }
    // 기본: 메뉴 전용 입력 (게임 입력 차단, 마우스 표시)
    return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, /*bHideCursor=*/ false);
}
```

#### 6.3.4 NativeOnActivated / NativeOnDeactivated 구현
```cpp
void UExWindowWidget::NativeOnActivated()
{
    Super::NativeOnActivated();
    // BP에서 열기 애니메이션 등 시각적 효과 처리
    BP_OnWindowOpened();
}

void UExWindowWidget::NativeOnDeactivated()
{
    // BP에서 닫기 애니메이션 등 시각적 효과 처리
    BP_OnWindowClosed();
    Super::NativeOnDeactivated();
}
```

#### 6.3.5 NativeGetDesiredFocusTarget 구현
```cpp
UWidget* UExWindowWidget::NativeGetDesiredFocusTarget() const
{
    // 게임패드 사용 시 이 위젯이 활성화되면 DefaultFocusWidgetName에 해당하는
    // 자식 위젯에 자동으로 포커스를 이동시킨다.
    if (!DefaultFocusWidgetName.IsNone())
    {
        return GetWidgetFromName(DefaultFocusWidgetName);
    }
    return Super::NativeGetDesiredFocusTarget();
}
```

#### 6.3.6 하위 클래스 확장 예시
```
UExWindowWidget (ExCore 베이스)
  ├── UExInventoryWindowWidget (ExCore 또는 GameFeature)
  │     - 인벤토리 전용 탭 리스트 통합
  │     - bAllowGameInputWhileOpen = false
  │
  ├── UExSettingsWindowWidget (ExCore)
  │     - Video / Audio / Gameplay 탭
  │     - bAllowGameInputWhileOpen = false
  │
  └── UExRunnerPauseMenuWidget (ExRunnerPlay)
        - 러너 전용 일시정지 메뉴
        - Resume / Settings / Quit 버튼
```

#### 6.3.7 포함해야 할 헤더
```cpp
#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "ExWindowWidget.generated.h"
```

---

### 6.4 ③ UExModalWidget — 팝업형 (Modal / Modeless) Layout용

| 항목 | 값 |
|---|---|
| 클래스명 | `UExModalWidget` |
| 부모 클래스 | `UCommonActivatableWidget` |
| 배치 위치 | `Plugins/GameFeatures/ExCore/Source/ExCoreRuntime/UI/Widgets/` |

#### 6.4.1 역할
- 확인/취소 대화상자, 경고 알림, 보상 획득 팝업, 연결 끊김 알림 등 **소형 오버레이** 위젯.
- `UIManagerSubsystem::PushModal()`을 통해 **MenuStack의 최상단에 Push**.
- **Modal 모드 (`bIsModal = true`, 기본값):** 입력을 완전히 독점한다. 뒤의 UI/게임이 입력을 받지 못한다.
- **Modeless 모드 (`bIsModal = false`):** 팝업이 떠 있지만 뒤의 게임/UI도 입력을 받을 수 있다. (예: 채팅 알림, 업적 팝업)
- 호출자가 결과(Confirm/Cancel/Custom)를 받을 수 있도록 **결과 델리게이트**를 내장한다.

#### 6.4.2 핵심 구현

```cpp
// 팝업 결과 열거형
UENUM(BlueprintType)
enum class EExModalResult : uint8
{
    Confirmed,
    Cancelled,
    Custom    // 확장용 (3개 이상의 버튼이 필요한 경우)
};

// 결과 델리게이트 (호출자가 구독)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnExModalResultReceived, EExModalResult, Result);

UCLASS(Abstract, BlueprintType, Blueprintable)
class EXCORERUNTIME_API UExModalWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
    virtual UWidget* NativeGetDesiredFocusTarget() const override;

    // 결과 델리게이트 — 호출자가 이 델리게이트를 구독하여 Confirm/Cancel 결과를 받는다.
    UPROPERTY(BlueprintAssignable, Category="ExUI|Modal")
    FOnExModalResultReceived OnModalResult;

    // 결과를 설정하고 위젯을 닫는다 (BP 또는 C++에서 호출)
    UFUNCTION(BlueprintCallable, Category="ExUI|Modal")
    void CloseWithResult(EExModalResult Result);

protected:
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;

    // BP 전환 효과 이벤트
    UFUNCTION(BlueprintImplementableEvent, Category="ExUI", meta=(DisplayName="On Popup Shown"))
    void BP_OnPopupShown();

    UFUNCTION(BlueprintImplementableEvent, Category="ExUI", meta=(DisplayName="On Popup Hidden"))
    void BP_OnPopupHidden();

    // Modal 여부 (true: 입력 독점, false: 입력 통과)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ExUI|Modal")
    bool bIsModal = true;

    // 배경 딤(Dim) 효과 활성화 여부
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ExUI|Modal")
    bool bShowBackgroundDim = true;

    // 딤 효과의 투명도 (0.0 = 투명, 1.0 = 완전 불투명)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ExUI|Modal", meta=(ClampMin="0.0", ClampMax="1.0", EditCondition="bShowBackgroundDim"))
    float BackgroundDimAlpha = 0.5f;

    // 게임패드 기본 포커스 대상 (보통 Confirm 버튼)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ExUI|Focus")
    FName DefaultFocusWidgetName = FName("ConfirmButton");

    // 결과가 설정되었는지 추적 (DeactivateWidget 시 결과 미설정이면 Cancelled 처리)
    bool bResultSet = false;
};
```

#### 6.4.3 GetDesiredInputConfig 구현
```cpp
TOptional<FUIInputConfig> UExModalWidget::GetDesiredInputConfig() const
{
    if (bIsModal)
    {
        // Modal: 메뉴 입력만 허용, 게임 입력 완전 차단
        return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, /*bHideCursor=*/ false);
    }
    // Modeless: 게임+UI 모두 입력 가능
    return FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::NoCapture, /*bHideCursor=*/ false);
}
```

#### 6.4.4 CloseWithResult 구현
```cpp
void UExModalWidget::CloseWithResult(EExModalResult Result)
{
    bResultSet = true;
    OnModalResult.Broadcast(Result);
    DeactivateWidget();  // CommonUI 스택에서 자동 제거
}
```

#### 6.4.5 NativeOnActivated / NativeOnDeactivated 구현
```cpp
void UExModalWidget::NativeOnActivated()
{
    Super::NativeOnActivated();
    bResultSet = false;
    BP_OnPopupShown();
}

void UExModalWidget::NativeOnDeactivated()
{
    // Back 입력 등으로 결과를 설정하지 않고 닫힌 경우 → Cancelled 처리
    if (!bResultSet)
    {
        OnModalResult.Broadcast(EExModalResult::Cancelled);
    }
    BP_OnPopupHidden();
    Super::NativeOnDeactivated();
}
```

#### 6.4.6 사용 패턴 예시 (호출자 코드)
```cpp
// 인벤토리 위젯에서 아이템 삭제 확인 팝업을 띄우는 예시
void UExInventoryWindowWidget::OnDeleteItemClicked()
{
    if (ULocalPlayer* LP = GetOwningLocalPlayer())
    {
        if (UExUIManagerSubsystem* UIManager = LP->GetSubsystem<UExUIManagerSubsystem>())
        {
            UCommonActivatableWidget* RawWidget = UIManager->PushModal(UExConfirmDialogWidget::StaticClass());
            if (UExModalWidget* Dialog = Cast<UExModalWidget>(RawWidget))
            {
                // 결과 구독
                Dialog->OnModalResult.AddDynamic(this, &UExInventoryWindowWidget::OnDeleteConfirmResult);
            }
        }
    }
}

void UExInventoryWindowWidget::OnDeleteConfirmResult(EExModalResult Result)
{
    if (Result == EExModalResult::Confirmed)
    {
        // 서버에 아이템 삭제 요청 (Server RPC)
        // ...
    }
}
```

#### 6.4.7 하위 클래스 확장 예시
```
UExModalWidget (ExCore 베이스)
  ├── UExConfirmDialogWidget (ExCore)
  │     - Confirm/Cancel 2버튼 대화상자
  │     - 제목/내용 텍스트를 UPROPERTY(meta=(BindWidget))로 노출
  │
  ├── UExRewardPopupWidget (ExCore 또는 GameFeature)
  │     - 보상 획득 팝업 (확인 버튼 1개)
  │     - bIsModal = true (보상을 반드시 확인하도록)
  │
  └── UExNotificationWidget (ExCore)
        - 채팅/업적 알림 토스트
        - bIsModal = false (게임 입력 차단 안 함)
        - 일정 시간 후 자동 닫힘 (타이머 사용)
```

#### 6.4.8 포함해야 할 헤더
```cpp
#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "ExModalWidget.generated.h"
```

---

### 6.5 베이스 버튼: UExBaseButtonWidget

| 항목 | 값 |
|---|---|
| 클래스명 | `UExBaseButtonWidget` |
| 부모 클래스 | `UCommonButtonBase` |
| 배치 위치 | `Plugins/GameFeatures/ExCore/Source/ExCoreRuntime/UI/Widgets/` |

#### 6.5.1 역할
- 프로젝트 전체에서 사용하는 **공용 버튼 스타일의 베이스**.
- `CommonButtonStyle` 에셋을 통해 전역 버튼 스타일(기본/호버/선택/눌림)을 일괄 관리한다.
- 디자이너가 이 클래스를 상속하여 메뉴 버튼, 탭 버튼, 아이콘 버튼 등 다양한 변형을 만든다.

#### 6.5.2 핵심 구현

```cpp
UCLASS(Abstract, BlueprintType, Blueprintable)
class EXCORERUNTIME_API UExBaseButtonWidget : public UCommonButtonBase
{
    GENERATED_BODY()

protected:
    // BP에서 레이블 텍스트를 설정할 수 있도록 노출
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<class UCommonTextBlock> ButtonLabel;

    // BP에서 액션 아이콘을 표시 (게임패드 프롬프트)
    // ★ 이름을 반드시 "InputActionWidget"으로 해야 CommonUI가 자동 인식 ★
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<class UCommonActionWidget> InputActionWidget;

public:
    // 버튼 텍스트 설정 (현지화 대응)
    UFUNCTION(BlueprintCallable, Category="ExUI|Button")
    void SetButtonText(const FText& InText);
};
```

#### 6.5.3 수동 에디터 단계 (Manual Editor Step)
1. 이 C++ 클래스를 기반으로 블루프린트 `WBP_ExBaseButton`을 생성한다.
2. 블루프린트 내부에 `UCommonTextBlock` (이름: `ButtonLabel`)과 `UCommonActionWidget` (이름: **반드시 `InputActionWidget`**)을 배치한다.
3. `CommonButtonStyle` 에셋을 생성하여 기본/호버/선택/눌림 상태의 시각적 스타일을 정의한다.
4. 프로젝트 전역에서 이 스타일을 사용한다.

#### 6.5.4 포함해야 할 헤더
```cpp
#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "ExBaseButtonWidget.generated.h"

// 전방 선언
class UCommonTextBlock;
class UCommonActionWidget;
```

### 6.6 검증 체크리스트
- [ ] `UExHUDLayoutWidget` 파생 BP를 Viewport에 추가하면 GameStack과 MenuStack이 `UExUIManagerSubsystem`에 등록된다.
- [ ] `PushWindow()`로 `UExWindowWidget` 파생 위젯을 열면 마우스 커서가 표시되고 게임 입력이 비활성화된다.
- [ ] 해당 창에서 Back(ESC/B 버튼) 입력 시 위젯이 Pop되고 게임 입력이 복원된다.
- [ ] `PushModal()`로 Modal 팝업을 열면 뒤의 Window 위젯도 입력을 받지 못한다.
- [ ] `PushModal()`로 Modeless 팝업(`bIsModal=false`)을 열면 뒤의 게임 입력이 유지된다.
- [ ] `CloseWithResult(Confirmed)`를 호출하면 `OnModalResult`가 브로드캐스트되고 위젯이 닫힌다.
- [ ] Back 입력으로 Modal을 닫으면 `EExModalResult::Cancelled`가 자동 브로드캐스트된다.
- [ ] 게임패드로 Window 위젯을 열면 `DefaultFocusWidgetName`에 해당하는 버튼에 포커스가 이동한다.
- [ ] 코드 어디에서도 `SetInputMode` 계열 함수가 사용되지 않았다.
- [ ] 코드 어디에서도 `ActivateWidget()`이 수동 호출되지 않았다.

---

## 7. 4단계 구현: MVVM ViewModel 베이스 및 연동

> **실행 지시:** 3단계 승인 후 이 단계를 구현한다. 사용자의 승인을 받은 후 5단계로 진행한다.

### 7.1 MVVM 개념 요약

```
Model (데이터 원본)          ViewModel (변환/가공)         View (UI 위젯)
─────────────────          ──────────────────           ─────────────
GameState                  UExMatchViewModel            HUD Widget
PlayerState        →       UExScoreViewModel      →     스코어보드
InventoryComponent         UExInventoryViewModel        인벤토리 창

                  OnRep/델리게이트          FieldNotify 바인딩
                  로 ViewModel에           으로 Widget이
                  값을 전달                자동 업데이트
```

**핵심 원칙:**
- View(Widget)는 Model(GameState/PlayerState 등)을 직접 참조하지 않는다.
- View는 ViewModel만 알고, ViewModel이 Model의 데이터를 가공하여 View에 전달한다.
- Model은 ViewModel의 존재를 알지 못한다. 델리게이트를 통해 간접적으로 데이터를 전달한다.

### 7.2 샘플 ViewModel: UExPlayerStatsViewModel

| 항목 | 값 |
|---|---|
| 클래스명 | `UExPlayerStatsViewModel` |
| 부모 클래스 | `UMVVMViewModelBase` |
| 배치 위치 | `Plugins/GameFeatures/ExCore/Source/ExCoreRuntime/UI/ViewModels/` |

#### 7.2.1 핵심 구현

```cpp
#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "ExPlayerStatsViewModel.generated.h"

UCLASS(BlueprintType)
class EXCORERUNTIME_API UExPlayerStatsViewModel : public UMVVMViewModelBase
{
    GENERATED_BODY()

public:
    // ── FieldNotify 프로퍼티 ──
    // ★ BlueprintReadOnly 사용 (ReadWrite가 아님!)
    // ReadWrite로 하면 BP에서 Setter를 거치지 않고 직접 변경하여 Broadcast가 누락된다.

    UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Setter)
    float CurrentHealth = 100.f;

    UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Setter)
    float MaxHealth = 100.f;

    UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Setter)
    int32 CurrentScore = 0;

    // ── 파생 FieldNotify 함수 (ProgressBar용 퍼센트 등) ──
    // Pure, Const, 리턴값만, 파라미터 없음 → FieldNotify 함수 조건 충족
    UFUNCTION(BlueprintPure, FieldNotify)
    float GetHealthPercent() const;

public:
    // ── Getter ──
    float GetCurrentHealth() const { return CurrentHealth; }
    float GetMaxHealth() const { return MaxHealth; }
    int32 GetCurrentScore() const { return CurrentScore; }

    // ── Setter (Broadcast 포함) ──
    void SetCurrentHealth(float NewHealth);
    void SetMaxHealth(float NewMaxHealth);
    void SetCurrentScore(int32 NewScore);
};
```

#### 7.2.2 CPP 구현

```cpp
#include "ExPlayerStatsViewModel.h"

float UExPlayerStatsViewModel::GetHealthPercent() const
{
    if (MaxHealth <= 0.f) return 0.f;
    return CurrentHealth / MaxHealth;
}

void UExPlayerStatsViewModel::SetCurrentHealth(float NewHealth)
{
    // UE_MVVM_SET_PROPERTY_VALUE: 값이 실제로 변경되었을 때만 true 반환
    if (UE_MVVM_SET_PROPERTY_VALUE(CurrentHealth, NewHealth))
    {
        // CurrentHealth가 변경되면 파생 필드인 GetHealthPercent도 Broadcast
        UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetHealthPercent);
    }
}

void UExPlayerStatsViewModel::SetMaxHealth(float NewMaxHealth)
{
    if (UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, NewMaxHealth))
    {
        UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetHealthPercent);
    }
}

void UExPlayerStatsViewModel::SetCurrentScore(int32 NewScore)
{
    UE_MVVM_SET_PROPERTY_VALUE(CurrentScore, NewScore);
}
```

#### 7.2.3 Model → ViewModel 연결 방법

PlayerState의 `OnRep` 델리게이트를 ViewModel에 연결하는 예시:

```cpp
// HUD Widget 또는 PlayerController에서 연결
void UExRunnerHUDLayout::NativeOnActivated()
{
    Super::NativeOnActivated();

    // PlayerState에서 ViewModel으로 데이터 흐름 설정
    if (APlayerState* PS = GetOwningPlayerState())
    {
        if (AExPlayerStateBase* ExPS = Cast<AExPlayerStateBase>(PS))
        {
            ExPS->OnScoreChanged.AddDynamic(this, &UExRunnerHUDLayout::HandleScoreChanged);
        }
    }
}

void UExRunnerHUDLayout::HandleScoreChanged(int32 OldScore, int32 NewScore)
{
    if (PlayerStatsVM)
    {
        PlayerStatsVM->SetCurrentScore(NewScore);
        // ViewModel의 FieldNotify가 자동으로 바인딩된 Widget을 업데이트
    }
}
```

### 7.3 수동 에디터 단계: ViewModel과 Widget 연결 (Manual Editor Step)

C++ ViewModel을 UMG Widget과 연결하려면 에디터에서 다음 작업이 필요하다:

1. Widget Blueprint (예: `WBP_RunnerHUD`)를 연다.
2. `Graph → View Bindings` 패널을 연다 (MVVM 플러그인 활성화 시 표시됨).
3. `Add Viewmodel` 버튼을 클릭하고 `UExPlayerStatsViewModel`을 선택한다.
4. `Creation Type`을 설정한다:
   - `Create Instance`: Widget이 ViewModel 인스턴스를 직접 생성
   - `Manual`: 코드에서 ViewModel 인스턴스를 직접 설정
   - `Property Path`: 기존 프로퍼티에서 ViewModel을 가져옴
5. Widget 내의 UI 요소(TextBlock, ProgressBar 등)에 FieldNotify 프로퍼티를 바인딩한다:
   - 예: `HealthBar`의 `Percent` ← `UExPlayerStatsViewModel`의 `GetHealthPercent`
   - 예: `ScoreText`의 `Text` ← `UExPlayerStatsViewModel`의 `CurrentScore` (자동 FText 변환)

### 7.4 포함해야 할 헤더
```cpp
#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "ExPlayerStatsViewModel.generated.h"
```

### 7.5 검증 체크리스트
- [ ] `SetCurrentHealth(50.f)` 호출 시 바인딩된 ProgressBar가 자동으로 50%로 업데이트된다.
- [ ] `SetCurrentScore(100)` 호출 시 바인딩된 TextBlock이 자동으로 "100"으로 업데이트된다.
- [ ] `Event Tick`이 UI 업데이트에 사용되지 않는다.
- [ ] FieldNotify 프로퍼티가 `BlueprintReadOnly`로 선언되어 있다 (ReadWrite가 아님).
- [ ] Setter 내부에서 `UE_MVVM_SET_PROPERTY_VALUE` 매크로가 사용된다.
- [ ] 파생 필드(`GetHealthPercent`)가 관련 프로퍼티 변경 시 함께 Broadcast된다.

---

## 8. 5단계 구현: 하이브리드 위젯 예시 (C++ 로직 + BP 연동)

> **실행 지시:** 4단계 승인 후 이 단계를 구현한다.

### 8.1 예시: UExMainHUDWidget (HUD Layout 확장)

| 항목 | 값 |
|---|---|
| 클래스명 | `UExMainHUDWidget` |
| 부모 클래스 | `UExHUDLayoutWidget` |
| 배치 위치 | `Plugins/GameFeatures/ExCore/Source/ExCoreRuntime/UI/Widgets/` (또는 GameFeature별) |

#### 8.1.1 핵심 구현

```cpp
UCLASS(Abstract, BlueprintType, Blueprintable)
class EXCORERUNTIME_API UExMainHUDWidget : public UExHUDLayoutWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // ── BindWidget: BP에서 반드시 같은 이름으로 위젯을 배치해야 함 ──

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<class UTextBlock> HealthText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<class UExBaseButtonWidget> InventoryButton;

    UPROPERTY(meta = (BindWidgetOptional))  // Optional: 없어도 크래시 안 남
    TObjectPtr<class UTextBlock> ScoreText;

    // ── MVVM ViewModel 참조 ──

    UPROPERTY(BlueprintReadOnly, Category="ExUI|ViewModel")
    TObjectPtr<UExPlayerStatsViewModel> PlayerStatsVM;

private:
    UFUNCTION()
    void HandleInventoryButtonClicked();
};
```

#### 8.1.2 NativeConstruct 구현

```cpp
void UExMainHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // ★ 포인터 유효성 검사 필수 ★
    if (InventoryButton)
    {
        InventoryButton->OnClicked().AddUObject(this, &UExMainHUDWidget::HandleInventoryButtonClicked);
    }

    // ViewModel 인스턴스 생성 (또는 에디터 View Bindings에서 설정)
    PlayerStatsVM = NewObject<UExPlayerStatsViewModel>(this);
}

void UExMainHUDWidget::NativeDestruct()
{
    // 델리게이트 해제
    if (InventoryButton)
    {
        InventoryButton->OnClicked().RemoveAll(this);
    }
    Super::NativeDestruct();
}

void UExMainHUDWidget::HandleInventoryButtonClicked()
{
    // ★ ULocalPlayerSubsystem 접근 방식 ★
    // GetGameInstance()->GetSubsystem<>() 이 아님! (ULocalPlayerSubsystem이므로)
    if (ULocalPlayer* LP = GetOwningLocalPlayer())
    {
        if (UExUIManagerSubsystem* UIManager = LP->GetSubsystem<UExUIManagerSubsystem>())
        {
            UIManager->PushWindow(UExInventoryWindowWidget::StaticClass());
        }
    }
}
```

### 8.2 수동 에디터 단계 (Manual Editor Step)

1. `UExMainHUDWidget`을 기반으로 블루프린트 `WBP_MainHUD`를 생성한다.
2. 블루프린트 디자이너에서 다음 위젯을 정확한 이름으로 배치한다:
   - `UCommonActivatableWidgetStack` 이름: `GameStack`
   - `UCommonActivatableWidgetStack` 이름: `MenuStack`
   - `UTextBlock` 이름: `HealthText`
   - `UExBaseButtonWidget` 파생 이름: `InventoryButton`
   - `UTextBlock` 이름: `ScoreText` (Optional)
3. `View Bindings` 패널에서 `UExPlayerStatsViewModel`을 추가하고 프로퍼티를 바인딩한다.
4. `EscapeMenuClass`에 사용할 일시정지 메뉴 BP를 지정한다.
5. PlayerController 또는 GameMode에서 이 HUD를 Viewport에 추가하는 로직을 구현한다.

### 8.3 검증 체크리스트
- [ ] PIE 시작 시 HUD가 화면에 표시된다.
- [ ] 인벤토리 버튼 클릭 시 인벤토리 창이 MenuStack에 Push되고, 마우스 커서가 표시된다.
- [ ] ESC 입력 시 인벤토리 창이 Pop되고 게임 입력이 복원된다.
- [ ] `HealthText`가 ViewModel의 `CurrentHealth` 변경에 따라 자동 업데이트된다.
- [ ] 코드에서 `SetInputMode` 계열 함수가 사용되지 않았다.
- [ ] `NativeConstruct`에서 모든 포인터에 유효성 검사가 수행된다.
- [ ] `NativeDestruct`에서 모든 델리게이트가 해제된다.

---

## 9. 전체 시스템 상호작용 다이어그램

```
┌──────────────────────────────────────────────────────────────────────────┐
│                        CommonGameViewportClient                          │
│                   (입력 라우팅의 최상위 진입점)                            │
│                                                                          │
│   입력 → 최상단 Activatable Widget으로 라우팅                             │
└────────────┬─────────────────────────────────────────────────────────────┘
             │
             ▼
┌──────────────────────────────────────────────────────────────────────────┐
│  UExHUDLayoutWidget (항상 활성, Viewport 루트)                            │
│  GetDesiredInputConfig: Game 모드                                        │
│                                                                          │
│  ┌────────────────────────┐  ┌─────────────────────────────────────┐    │
│  │ GameStack              │  │ MenuStack                           │    │
│  │ (Z-Order: 하위)         │  │ (Z-Order: 상위)                     │    │
│  │                        │  │                                     │    │
│  │ - 미니맵               │  │ ┌─────────────────────────────┐    │    │
│  │ - 퀘스트 트래커         │  │ │ UExWindowWidget 파생         │    │    │
│  │ - 인게임 알림           │  │ │ (인벤토리, 설정 등)           │    │    │
│  │                        │  │ │ GetDesiredInputConfig: Menu  │    │    │
│  │                        │  │ └─────────────────────────────┘    │    │
│  │                        │  │ ┌─────────────────────────────┐    │    │
│  │                        │  │ │ UExModalWidget 파생          │    │    │
│  │                        │  │ │ (확인창, 보상 팝업 등)        │    │    │
│  │                        │  │ │ GetDesiredInputConfig:       │    │    │
│  │                        │  │ │   Modal→Menu / Modeless→All  │    │    │
│  │                        │  │ │ OnModalResult 델리게이트      │    │    │
│  │                        │  │ └─────────────────────────────┘    │    │
│  └────────────────────────┘  └─────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────────────────────┘
             │
             │  UIManagerSubsystem
             │  (ULocalPlayerSubsystem)
             │
             │  PushWindow() / PushModal() / PopMenu()
             │
┌────────────┴─────────────────────────────────────────────────────────────┐
│                        MVVM 데이터 흐름                                   │
│                                                                          │
│  서버 (GameMode)                                                         │
│    │                                                                     │
│    ├→ GameState (복제) → OnRep → UExMatchViewModel → HUD 자동 업데이트   │
│    │                                                                     │
│    └→ PlayerState (복제) → OnRep → UExPlayerStatsViewModel               │
│                                           → HealthBar 자동 업데이트      │
│                                           → ScoreText 자동 업데이트      │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 10. 파일 생성 순서 요약

### 1단계: 환경 설정 (코드 없음, 에디터 작업)
- ExCore `.Build.cs` 모듈 추가
- 에디터 프로젝트 설정 (4.2절 전체)

### 2단계: UI 매니저 (1단계 완료 후)
1. `ExCore/Source/ExCoreRuntime/UI/Subsystems/ExUIManagerSubsystem.h` / `.cpp`

### 3단계: 3분류 베이스 위젯 (2단계 승인 후)
2. `ExCore/Source/ExCoreRuntime/UI/Widgets/ExHUDLayoutWidget.h` / `.cpp`
3. `ExCore/Source/ExCoreRuntime/UI/Widgets/ExWindowWidget.h` / `.cpp`
4. `ExCore/Source/ExCoreRuntime/UI/Widgets/ExModalWidget.h` / `.cpp`
5. `ExCore/Source/ExCoreRuntime/UI/Widgets/ExBaseButtonWidget.h` / `.cpp`

### 4단계: MVVM ViewModel (3단계 승인 후)
6. `ExCore/Source/ExCoreRuntime/UI/ViewModels/ExPlayerStatsViewModel.h` / `.cpp`

### 5단계: 하이브리드 예시 (4단계 승인 후)
7. `ExCore/Source/ExCoreRuntime/UI/Widgets/ExMainHUDWidget.h` / `.cpp`
