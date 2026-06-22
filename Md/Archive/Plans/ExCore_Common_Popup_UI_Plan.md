# ExCore 공통 팝업 UI 시스템 개발 계획서

ExCore 플러그인에 **CommonUI 기반 + Descriptor 데이터 드리븐**의 공통 팝업 UI 시스템을 구축한다. 기존 `UExModalWidget`(CommonUI `UCommonActivatableWidget` 기반)을 상속하여 구현하므로, CommonUI의 입력 라우팅/스택 관리/게임패드 지원이 모두 자동 적용된다. 모든 하위 GameFeature 플러그인(ExRunnerPlay 등)에서 일관된 인터페이스로 팝업을 호출하고, 필요에 따라 확장할 수 있는 구조를 제공한다.

> **셀업 가이드:** `Guides/ExCore/ExCore_Common_Popup_UI_Setup_Guide.md`  
> **UI 기반 아키텍처:** `Architecture/ExCore/ExFrameWork_UI_System_Architecture.md` (v2.1) — CommonUI + MVVM

---

## User Review Required

> [!IMPORTANT]
> **Toast 위젯 배치 위치 결정**: Toast는 Modal 스택(MenuStack)에 Push하지 않고 HUD Layout에 별도 컨테이너(VerticalBox 등)를 두는 방식을 제안한다. Toast가 여러 개 동시에 표시되어야 하므로 스택 기반이 아닌 큐(Queue) 기반이 적합하다. HUD Layout BP에 Toast 컨테이너를 추가하는 것을 승인해야 한다.

> [!IMPORTANT]
> **RichText 스타일 DataTable 프리셋**: ExCore에서 기본 제공할 RichText 스타일 태그(Default, Bold, Emphasis, Warning, Success)의 색상/폰트 사양을 최종 확정해야 한다. 계획서에는 골격만 정의하며, 실제 색상값은 프로젝트 아트 디렉션에 맞춰 BP에서 조정한다.

> [!IMPORTANT]
> **Toast 최대 동시 표시 개수**: 화면에 동시에 몇 개의 Toast를 표시할지 결정이 필요하다. 기본 제안값은 3개이며, 초과 시 가장 오래된 것부터 제거한다.

> [!WARNING]
> **기존 UExModalWidget과의 관계**: 기존 UI 아키텍처 문서(v2.1)에 정의된 `UExModalWidget`은 팝업의 "컨테이너(껍데기)" 역할이다. 이 계획서의 `UExPopupWidget`은 `UExModalWidget`을 **상속**하여, 그 위에 Descriptor 기반 콘텐츠 구성 기능을 추가하는 구조이다. 기존 `UExModalWidget`의 `bIsModal`, `bShowBackgroundDim`, `CloseWithResult()`, `OnModalResult` 등은 그대로 보존된다.

---

## 의존 문서

- `ExFrameWork_UI_System_Architecture.md (v2.1)` — 위젯 3분류 체계, UExModalWidget 정의, UIManagerSubsystem
- `ExFrameWork_Multiplayer_Flow_Architecture.md (v2.0)` — UI 데이터 흐름 원칙

---

## 시스템 아키텍처 개요

### 계층 구조

```
UCommonActivatableWidget (CommonUI 제공)
  └── UExModalWidget (ExCore — 기존, 팝업 컨테이너)
        │   bIsModal, bShowBackgroundDim, CloseWithResult(), OnModalResult
        │
        └── UExPopupWidget (ExCore — 신규, Descriptor 기반 팝업)
              │   FExPopupDescriptor 수신 → 동적 콘텐츠 구성
              │   UCommonRichTextBlock* Text_Title  (BindWidget)
              │   UCommonRichTextBlock* Text_Body   (BindWidget)
              │   UPanelWidget* Panel_Buttons       (BindWidget)
              │   UPanelWidget* Panel_Input          (BindWidget, Optional)
              │   UEditableTextBox* EditBox_Input    (BindWidget, Optional)
              │
              └── [하위 플러그인 확장] (ExRunnerPlay 등)
                    커스텀 팝업 서브클래스 (필요 시)

UCommonUserWidget (CommonUI 제공 — 비모달, 비스택)
  └── UExToastWidget (ExCore — 신규, Toast 전용)
        │   FExToastDescriptor 수신 → Toast 콘텐츠 구성
        │   UCommonRichTextBlock* Text_Message (BindWidget)
        │   UProgressBar* ProgressBar_Timer    (BindWidget, Optional)
        │   FOnToastClosed 델리게이트 → 서브시스템에 닫힘 알림
        │
        └── [하위 플러그인 확장] (필요 시)
```

### 호출 흐름

```
[호출자: C++ 또는 BP]
     │
     ▼
UExUIManagerSubsystem
  ├── ShowInfo(Title, Body)              → PopupType::Info
  ├── ShowAcknowledge(Title, Body, CB)   → PopupType::Acknowledge  
  ├── ShowConfirm(Title, Body, CB)       → PopupType::Confirm
  ├── ShowInputPrompt(Title, Body, CB)   → PopupType::InputPrompt
  ├── ShowPopup(FExPopupDescriptor, CB)  → 고급 사용자용 직접 조립
  │     │
  │     ▼
  │   MenuStack->AddWidget<UExPopupWidget>()
  │     → UExPopupWidget::InitFromDescriptor(Desc)
  │       → 타입에 따라 버튼/에디터박스 Visibility 제어
  │       → 버튼 동적 생성 → 클릭 시 CloseWithResult() 호출
  │
  ├── ShowToast(Message, Duration)            → 단순 Toast
  ├── ShowTimedToast(Message, Duration, CB)   → 프로그레스바 Toast
  └── ShowLoadingToast(Message)               → 수동 프로그레스 Toast
        │
        ▼
      HUDLayout의 ToastContainer에 AddChild
        → UExToastWidget 인스턴스 생성 및 큐 관리
        → Toast 닫힘 시 OnToastClosed 델리게이트로 ActiveToasts 자동 갱신
```

---

## Descriptor 데이터 구조

### 설계 원칙: 팝업/Toast Descriptor 분리

팝업(Modal 스택에 Push되는 상호작용 UI)과 Toast(HUD에 큐로 쌓이는 단방향 알림)는 동작 원리가 완전히 다르다. 따라서 Descriptor를 분리하여 각 구조체가 자기 역할에 맞는 필드만 보유하도록 한다.

- **`FExPopupDescriptor`** — 모달형 팝업 전용 (Info / Acknowledge / Confirm / InputPrompt)
- **`FExToastDescriptor`** — Toast 전용 (메시지, Duration, 프로그레스 설정)

### EExPopupType (팝업 타입 Enum — Toast 제외)

```cpp
UENUM(BlueprintType)
enum class EExPopupType : uint8
{
    Info          UMETA(DisplayName="정보 (텍스트만)"),
    Acknowledge   UMETA(DisplayName="확인 (버튼 1개)"),
    Confirm       UMETA(DisplayName="확인/취소 (버튼 2개)"),
    InputPrompt   UMETA(DisplayName="텍스트 입력 (에디터박스 + 버튼)")
};
```

### FExPopupButtonDesc (버튼 정보)

```cpp
USTRUCT(BlueprintType)
struct EXCORERUNTIME_API FExPopupButtonDesc
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup|Button")
    FText Label;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup|Button")
    EExModalResult ResultValue = EExModalResult::Confirmed;
};
```

### FExPopupInputDesc (에디터박스 설정)

```cpp
USTRUCT(BlueprintType)
struct EXCORERUNTIME_API FExPopupInputDesc
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup|Input")
    FText PlaceholderText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup|Input",
        meta=(ClampMin="0", ClampMax="1000"))
    int32 MaxLength = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup|Input")
    bool bIsPassword = false;
};
```

### FExPopupDescriptor (모달형 팝업 전용 Descriptor)

```cpp
USTRUCT(BlueprintType)
struct EXCORERUNTIME_API FExPopupDescriptor
{
    GENERATED_BODY()

    // ─── 팝업 타입 (에디터에서 이 값에 따라 하위 필드 표시/숨김) ───
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup")
    EExPopupType PopupType = EExPopupType::Info;

    // ─── 공통 필드 (모든 모달형 팝업에서 표시) ───
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup|Content")
    FText Title;

    // RichText 마크업 사용 가능: <Bold>강조</>, <Warning>경고</>
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup|Content")
    FText Body;

    // ─── 버튼 배열 (Acknowledge, Confirm, InputPrompt에서만 표시) ───
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup|Buttons",
        meta=(EditCondition="PopupType == EExPopupType::Acknowledge || PopupType == EExPopupType::Confirm || PopupType == EExPopupType::InputPrompt",
              EditConditionHides))
    TArray<FExPopupButtonDesc> Buttons;

    // ─── 에디터박스 설정 (InputPrompt에서만 표시) ───
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup|Input",
        meta=(EditCondition="PopupType == EExPopupType::InputPrompt",
              EditConditionHides))
    FExPopupInputDesc InputConfig;

    // ─── 자동 닫힘 (Info에서만 표시) ───
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup|AutoClose",
        meta=(EditCondition="PopupType == EExPopupType::Info",
              EditConditionHides))
    float AutoCloseSeconds = 3.0f;

    // ─── Modal 여부 ───
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Popup|Behavior")
    bool bIsModal = true;
};
```

### 에디터 디테일 패널 EditCondition 동작 요약

| PopupType 선택 | Title | Body | Buttons | InputConfig | AutoCloseSeconds | bIsModal |
|---|---|---|---|---|---|---|
| **Info** | ✅ 표시 | ✅ 표시 | ❌ 숨김 | ❌ 숨김 | ✅ 표시 | ✅ 표시 |
| **Acknowledge** | ✅ 표시 | ✅ 표시 | ✅ 표시 | ❌ 숨김 | ❌ 숨김 | ✅ 표시 |
| **Confirm** | ✅ 표시 | ✅ 표시 | ✅ 표시 | ❌ 숨김 | ❌ 숨김 | ✅ 표시 |
| **InputPrompt** | ✅ 표시 | ✅ 표시 | ✅ 표시 | ✅ 표시 | ❌ 숨김 | ✅ 표시 |

> **참고:** `EditCondition` + `EditConditionHides`는 에디터 디테일 패널 전용 UX 기능이다. 런타임 C++ 코드에서 `FExPopupDescriptor`를 직접 조립할 때는 모든 필드에 자유롭게 접근 가능하며, 이 숨김/보임은 런타임 로직에 영향을 주지 않는다.

### FExToastDescriptor (Toast 전용 Descriptor)

```cpp
USTRUCT(BlueprintType)
struct EXCORERUNTIME_API FExToastDescriptor
{
    GENERATED_BODY()

    // Toast 메시지 (RichText 마크업 사용 가능)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toast")
    FText Message;

    // 자동 닫힘 시간 (초). 0이면 수동 닫기 전까지 유지
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toast",
        meta=(ClampMin="0.0"))
    float Duration = 3.0f;

    // 프로그레스바 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toast|Progress")
    FExToastProgressConfig ProgressConfig;
};
```

### FExToastProgressConfig (Toast 프로그레스 설정)

```cpp
USTRUCT(BlueprintType)
struct EXCORERUNTIME_API FExToastProgressConfig
{
    GENERATED_BODY()

    // 프로그레스바 표시 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toast|Progress")
    bool bShowProgressBar = false;

    // true: Duration에 연동하여 자동 감소
    // false: 외부에서 SetProgress()로 수동 제어 (로딩용)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toast|Progress",
        meta=(EditCondition="bShowProgressBar", EditConditionHides))
    bool bAutoCountdown = true;
};
```

---

## 콜백 이벤트 시스템 (C++ / BP 이중 제공)

### 델리게이트 선언

```cpp
// ──── BP 전용: Dynamic Multicast (블루프린트에서 바인딩 가능) ────
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnExPopupResultBP,
    EExModalResult, Result,
    const FText&, InputText);

// ──── C++ 전용: Native Delegate (람다/멤버 함수 바인딩) ────
DECLARE_DELEGATE_TwoParams(
    FOnExPopupResultNative,
    EExModalResult /*Result*/,
    const FText& /*InputText*/);
```

### UExPopupWidget 내부 델리게이트

```cpp
UCLASS(Abstract, BlueprintType, Blueprintable)
class EXCORERUNTIME_API UExPopupWidget : public UExModalWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="ExUI|Popup")
    void InitFromDescriptor(const FExPopupDescriptor& Descriptor);

    UPROPERTY(BlueprintAssignable, Category="ExUI|Popup")
    FOnExPopupResultBP OnPopupResult;

    FOnExPopupResultNative OnPopupResultNative;

    UFUNCTION(BlueprintPure, Category="ExUI|Popup")
    FText GetInputText() const;

protected:
    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UCommonRichTextBlock> Text_Title;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UCommonRichTextBlock> Text_Body;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UPanelWidget> Panel_Buttons;

    UPROPERTY(meta=(BindWidgetOptional))
    TObjectPtr<UPanelWidget> Panel_Input;

    UPROPERTY(meta=(BindWidgetOptional))
    TObjectPtr<UEditableTextBox> EditBox_Input;

    FTimerHandle AutoCloseTimerHandle;

    UPROPERTY(BlueprintReadOnly, Category="ExUI|Popup")
    FExPopupDescriptor CurrentDescriptor;

    void SetupButtons(const TArray<FExPopupButtonDesc>& ButtonDescs);
    void SetupInputField(const FExPopupInputDesc& InputDesc);
    void StartAutoCloseTimer(float Seconds);

    UFUNCTION()
    void OnButtonClicked(EExModalResult Result);

    virtual void CloseWithResult(EExModalResult Result) override;
};
```

### CloseWithResult 오버라이드 — 이중 델리게이트 브로드캐스트

```cpp
void UExPopupWidget::CloseWithResult(EExModalResult Result)
{
    FText InputText = FText::GetEmpty();
    if (EditBox_Input)
    {
        InputText = EditBox_Input->GetText();
    }

    // 1) BP 델리게이트 브로드캐스트
    OnPopupResult.Broadcast(Result, InputText);

    // 2) C++ Native 델리게이트 실행
    OnPopupResultNative.ExecuteIfBound(Result, InputText);

    // 3) 부모의 CloseWithResult 호출 (OnModalResult 브로드캐스트 + DeactivateWidget)
    Super::CloseWithResult(Result);
}
```

### UIManagerSubsystem 편의 함수

```cpp
// ──── C++ 호출자용 (람다 콜백) ────
void ShowInfo(const FText& Title, const FText& Body, float AutoCloseSeconds = 3.0f);

void ShowAcknowledge(const FText& Title, const FText& Body,
                     FOnExPopupResultNative OnResult = nullptr);

void ShowConfirm(const FText& Title, const FText& Body,
                 FOnExPopupResultNative OnResult);

void ShowInputPrompt(const FText& Title, const FText& Body,
                     const FExPopupInputDesc& InputConfig,
                     FOnExPopupResultNative OnResult);

void ShowPopup(const FExPopupDescriptor& Descriptor,
               FOnExPopupResultNative OnResult = nullptr);

// ──── BP 호출자용 (반환된 위젯의 OnPopupResult에 직접 바인딩) ────

UFUNCTION(BlueprintCallable, Category="ExUI|Popup", meta=(DisplayName="Show Info Popup"))
UExPopupWidget* ShowInfoBP(const FText& Title, const FText& Body, float AutoCloseSeconds = 3.0f);

UFUNCTION(BlueprintCallable, Category="ExUI|Popup", meta=(DisplayName="Show Acknowledge Popup"))
UExPopupWidget* ShowAcknowledgeBP(const FText& Title, const FText& Body);

UFUNCTION(BlueprintCallable, Category="ExUI|Popup", meta=(DisplayName="Show Confirm Popup"))
UExPopupWidget* ShowConfirmBP(const FText& Title, const FText& Body);

UFUNCTION(BlueprintCallable, Category="ExUI|Popup", meta=(DisplayName="Show Input Prompt Popup"))
UExPopupWidget* ShowInputPromptBP(const FText& Title, const FText& Body,
                                   const FExPopupInputDesc& InputConfig);

UFUNCTION(BlueprintCallable, Category="ExUI|Popup", meta=(DisplayName="Show Popup (Advanced)"))
UExPopupWidget* ShowPopupBP(const FExPopupDescriptor& Descriptor);
```

### C++ 호출 예시

```cpp
// GameFeature 플러그인에서 확인 팝업 호출
void AExRunnerPlayerController::OnQuitRequested()
{
    if (UExUIManagerSubsystem* UIMgr = GetLocalPlayer()->GetSubsystem<UExUIManagerSubsystem>())
    {
        // ⚠️ 반드시 CreateWeakLambda를 사용하여 호출자 파괴 시 안전하게 처리
        UIMgr->ShowConfirm(
            LOCTEXT("QuitTitle", "게임 종료"),
            LOCTEXT("QuitBody", "정말 <Warning>종료</>하시겠습니까?\n저장되지 않은 진행 상황은 사라집니다."),
            FOnExPopupResultNative::CreateWeakLambda(this, [this](EExModalResult Result, const FText& /*InputText*/)
            {
                if (Result == EExModalResult::Confirmed)
                {
                    UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
                }
            })
        );
    }
}
```

### BP 호출 흐름 (개념)

```
1. Get UIManagerSubsystem → ShowConfirmBP("제목", "본문")
2. 반환값 UExPopupWidget* → OnPopupResult 이벤트에 바인딩 (Assign)
3. 이벤트 수신 시 Result == Confirmed 분기 처리
```

---

## RichText 기반 구조

### 텍스트 위젯 선택: UCommonRichTextBlock

- `UCommonRichTextBlock`은 `URichTextBlock`의 서브클래스 (CommonUI 제공)
- 모바일 플랫폼용 커스텀 스케일링 자동 적용
- DataTable 기반 스타일 태그 시스템으로 색상, 굵기, 크기 제어

### ExCore 공통 RichText 스타일 DataTable

**에셋 경로:** `ExCore/Content/UI/Data/DT_ExRichTextStyles`
**Row Structure:** `RichTextStyleRow`

| Row Name (태그) | 용도 | 스타일 기본값 |
|---|---|---|
| `Default` | 기본 본문 텍스트 | 프로젝트 기본 폰트, 흰색/밝은 회색 |
| `Bold` | 강조 굵은 글씨 | Bold Weight, 기본 색상 유지 |
| `Emphasis` | 중요 강조 | Bold, 하이라이트 색상 (노랑/금색 계열) |
| `Warning` | 경고/위험 표시 | Bold, 빨강/주황 |
| `Success` | 성공/완료 표시 | Regular, 초록 |
| `Subtle` | 부가 정보 (작은 글씨) | Regular, 회색, 작은 사이즈 |

### 마크업 사용 예시

```
일반 텍스트 <Bold>굵은 텍스트</> 다시 일반
<Warning>주의: 이 작업은 되돌릴 수 없습니다.</>
<Emphasis>100골드</>를 획득했습니다!
<Success>저장 완료!</> 계속 진행하세요.
```

### Build.cs 추가 필요 모듈 (RichText 관련)

```csharp
// ExCore.Build.cs — 기존 UMG, CommonUI, CommonInput, ModelViewViewModel에 추가
PublicDependencyModuleNames.AddRange(new string[]
{
    "Slate",        // RichTextBlock 내부적으로 Slate 사용
    "SlateCore",    // SRichTextBlock 위젯
});
```

### 수동 에디터 단계 (RichText 설정)

1. `ExCore/Content/UI/Data/` 경로에 DataTable 에셋 생성 → Row Structure: `RichTextStyleRow`
2. 위 표의 행(Row)들을 추가하고 폰트/색상 설정
3. `WBP_ExPopupWidget` (팝업 BP)의 `Text_Title`과 `Text_Body` 위젯 디테일에서:
   - `Text Style Set` → 위에서 만든 `DT_ExRichTextStyles` 할당
4. `WBP_ExToastWidget` (Toast BP)의 `Text_Message` 위젯에도 동일하게 할당

---

## Toast 시스템 상세

### UExToastWidget 클래스

```cpp
// Toast 닫힘 시 서브시스템에 알리기 위한 Native 델리게이트
// Toast → 서브시스템 직접 참조 없이 델리게이트 콜백으로 느슨한 결합 유지
DECLARE_DELEGATE_OneParam(FOnToastClosed, UExToastWidget* /*ClosedToast*/);

UCLASS(Abstract, BlueprintType, Blueprintable)
class EXCORERUNTIME_API UExToastWidget : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    // Toast 초기화 (FExToastDescriptor 기반)
    UFUNCTION(BlueprintCallable, Category="ExUI|Toast")
    void InitToast(const FExToastDescriptor& Descriptor);

    // 외부에서 프로그레스 수동 제어 (로딩 Toast용)
    UFUNCTION(BlueprintCallable, Category="ExUI|Toast")
    void SetProgress(float NormalizedValue);  // 0.0 ~ 1.0

    // 수동 닫기
    UFUNCTION(BlueprintCallable, Category="ExUI|Toast")
    void CloseToast();

    // 결과 델리게이트 (타이머 만료 시 Confirmed, 수동 닫기 시 Cancelled)
    UPROPERTY(BlueprintAssignable, Category="ExUI|Toast")
    FOnExPopupResultBP OnToastFinished;

    FOnExPopupResultNative OnToastFinishedNative;

    // ── 서브시스템 전용: Toast 닫힘 알림 (서브시스템이 바인딩) ──
    // UPROPERTY() 없음 — 서브시스템이 추적용으로만 사용
    FOnToastClosed OnToastClosed;

protected:
    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UCommonRichTextBlock> Text_Message;

    UPROPERTY(meta=(BindWidgetOptional))
    TObjectPtr<UProgressBar> ProgressBar_Timer;

    UFUNCTION(BlueprintImplementableEvent, Category="ExUI|Toast", meta=(DisplayName="On Toast Shown"))
    void BP_OnToastShown();

    UFUNCTION(BlueprintImplementableEvent, Category="ExUI|Toast", meta=(DisplayName="On Toast Hidden"))
    void BP_OnToastHidden();

    float TotalDuration = 0.0f;
    float ElapsedTime = 0.0f;
    bool bAutoCountdown = true;
    FTimerHandle CountdownTimerHandle;

    void TickCountdown(float DeltaTime);
};
```

### CloseToast 구현 — 닫힘 알림 포함

```cpp
void UExToastWidget::CloseToast()
{
    if (CountdownTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(CountdownTimerHandle);
    }

    BP_OnToastHidden();

    // 서브시스템에 닫힘 알림 (ActiveToasts 배열에서 자신을 제거하도록)
    OnToastClosed.ExecuteIfBound(this);

    // 결과 브로드캐스트
    const EExModalResult Result = (ElapsedTime >= TotalDuration)
        ? EExModalResult::Confirmed   // 시간 만료
        : EExModalResult::Cancelled;  // 수동 닫기 또는 강제 제거
    OnToastFinished.Broadcast(Result, FText::GetEmpty());
    OnToastFinishedNative.ExecuteIfBound(Result, FText::GetEmpty());

    RemoveFromParent();
}
```

### Toast 큐 관리 (UIManagerSubsystem 확장)

```cpp
// ── Toast 컨테이너 참조 ──
// UPROPERTY()로 강참조 유지, 레벨 전환 시 RegisterToastContainer(nullptr)로 해제
UPROPERTY()
TObjectPtr<UPanelWidget> ToastContainer;

// ── 활성 Toast 추적 ──
// UPROPERTY() 없음 — Toast의 GC 소유권은 HUD(UPanelWidget)에 있음
// 서브시스템은 추적만 하므로 약참조 사용
TArray<TWeakObjectPtr<UExToastWidget>> ActiveToasts;

// Toast 최대 동시 표시 수
UPROPERTY(EditDefaultsOnly, Category="ExUI|Toast")
int32 MaxVisibleToasts = 3;

UFUNCTION(BlueprintCallable, Category="ExUI|Toast")
void RegisterToastContainer(UPanelWidget* InToastContainer);

// Toast 닫힘 콜백 (UExToastWidget::OnToastClosed에 바인딩)
void HandleToastClosed(UExToastWidget* ClosedToast);
```

### HandleToastClosed 구현

```cpp
void UExUIManagerSubsystem::HandleToastClosed(UExToastWidget* ClosedToast)
{
    // 만료된 WeakPtr 및 닫힌 Toast를 배열에서 제거
    ActiveToasts.RemoveAll([ClosedToast](const TWeakObjectPtr<UExToastWidget>& Weak)
    {
        return !Weak.IsValid() || Weak.Get() == ClosedToast;
    });
}
```

### Toast 생성 및 큐 관리 로직

```cpp
UExToastWidget* UExUIManagerSubsystem::ShowToast(const FText& Message, float Duration)
{
    if (!ToastContainer)
    {
        UE_LOG(LogExUI, Warning, TEXT("ShowToast 실패: ToastContainer가 등록되지 않음"));
        return nullptr;
    }

    // 만료된 WeakPtr 정리 (안전성 보장)
    ActiveToasts.RemoveAll([](const TWeakObjectPtr<UExToastWidget>& Weak)
    {
        return !Weak.IsValid();
    });

    // 최대 표시 수 초과 시 가장 오래된 것 제거
    while (ActiveToasts.Num() >= MaxVisibleToasts)
    {
        if (ActiveToasts[0].IsValid())
        {
            ActiveToasts[0]->CloseToast();  // → HandleToastClosed → 배열에서 제거
        }
        else
        {
            ActiveToasts.RemoveAt(0);
        }
    }

    // Toast 위젯 생성
    UExToastWidget* Toast = CreateWidget<UExToastWidget>(
        GetLocalPlayer()->GetPlayerController(GetWorld()), ToastWidgetClass);
    if (Toast)
    {
        FExToastDescriptor Desc;
        Desc.Message = Message;
        Desc.Duration = Duration;

        Toast->InitToast(Desc);

        // 닫힘 알림 바인딩 (델리게이트로 느슨한 결합)
        Toast->OnToastClosed.BindUObject(this, &UExUIManagerSubsystem::HandleToastClosed);

        ToastContainer->AddChild(Toast);
        ActiveToasts.Add(Toast);
    }
    return Toast;
}
```

### Toast 프로그레스바 동작

**자동 카운트다운 모드 (`bAutoCountdown = true`):**
- `InitToast()` 호출 시 타이머 시작
- 매 Tick마다 `ElapsedTime += DeltaTime`
- `ProgressBar_Timer->SetPercent(1.0f - (ElapsedTime / TotalDuration))`
- `ElapsedTime >= TotalDuration`이면 `CloseToast()` 호출

**수동 제어 모드 (`bAutoCountdown = false`):**
- 자동 타이머 비활성
- 호출자가 `SetProgress(0.0~1.0)`으로 직접 진행률 제어
- 호출자가 작업 완료 시 `CloseToast()` 수동 호출

### UIManagerSubsystem Toast 편의 함수

```cpp
// 단순 Toast — 프로그레스바 없음
UFUNCTION(BlueprintCallable, Category="ExUI|Toast")
UExToastWidget* ShowToast(const FText& Message, float Duration = 3.0f);

// 타이머 Toast — 자동 감소 프로그레스바 표시
UFUNCTION(BlueprintCallable, Category="ExUI|Toast")
UExToastWidget* ShowTimedToast(const FText& Message, float Duration = 3.0f);

// 로딩 Toast — 수동 프로그레스 제어, 호출자가 닫기
UFUNCTION(BlueprintCallable, Category="ExUI|Toast")
UExToastWidget* ShowLoadingToast(const FText& Message);

// 고급: Toast Descriptor 직접 조립
UFUNCTION(BlueprintCallable, Category="ExUI|Toast")
UExToastWidget* ShowToastFromDescriptor(const FExToastDescriptor& Descriptor);
```

---

## 파일 구조 및 배치

### ExCore 플러그인 내 파일 배치

```
Plugins/GameFeatures/ExCore/
├── Source/ExCoreRuntime/
│   └── UI/
│       ├── Data/
│       │   ├── ExPopupTypes.h               ← EExPopupType, EExModalResult (기존)
│       │   ├── ExPopupDescriptor.h           ← FExPopupDescriptor, FExPopupButtonDesc,
│       │   │                                    FExPopupInputDesc
│       │   ├── ExToastDescriptor.h           ← FExToastDescriptor, FExToastProgressConfig
│       │   ├── ExPopupDescriptor.cpp
│       │   └── ExToastDescriptor.cpp
│       │
│       ├── Widgets/
│       │   ├── ExModalWidget.h / .cpp        ← 기존 (변경 없음)
│       │   ├── ExPopupWidget.h / .cpp        ← 신규: Descriptor 기반 팝업
│       │   └── ExToastWidget.h / .cpp        ← 신규: Toast 전용 위젯
│       │
│       └── Subsystems/
│           └── ExUIManagerSubsystem.h / .cpp ← 기존 확장: 팝업/Toast 편의 함수 추가
│
└── Content/
    └── UI/
        ├── Data/
        │   └── DT_ExRichTextStyles.uasset    ← RichText 스타일 DataTable
        │
        └── Widgets/
            ├── WBP_ExPopupWidget.uasset       ← UExPopupWidget BP 서브클래스
            └── WBP_ExToastWidget.uasset       ← UExToastWidget BP 서브클래스
```

---

## 구현 단계

### 1단계: 데이터 구조 및 Enum 정의

**작업 내용:**
1. `ExPopupTypes.h` — `EExPopupType` Enum 추가 (Toast 제외, 4개 값)
2. `ExPopupDescriptor.h/.cpp` — 모달형 팝업 구조체 정의
3. `ExToastDescriptor.h/.cpp` — Toast 전용 구조체 정의
4. `ExCore.Build.cs`에 `Slate`, `SlateCore` 모듈 추가

**검증:**
- [ ] `FExPopupDescriptor` 인스턴스화 및 컴파일 정상
- [ ] `FExToastDescriptor` 인스턴스화 및 컴파일 정상
- [ ] DataTable에서 `FExPopupDescriptor` Row 사용 시 `EditConditionHides` 동작

### 2단계: UExPopupWidget 구현

**작업 내용:**
1. `UExPopupWidget` 클래스 작성 (`UExModalWidget` 상속)
2. `InitFromDescriptor()` — 타입별 Visibility 분기, 버튼 동적 생성, 에디터박스 설정, 자동 닫힘 타이머
3. `CloseWithResult()` 오버라이드 — BP/Native 이중 델리게이트 브로드캐스트
4. `GetInputText()` 구현

**검증:**
- [ ] `Acknowledge` 타입: 버튼 1개 표시, 에디터박스 숨김
- [ ] `Confirm` 타입: 확인→Confirmed, 취소→Cancelled 브로드캐스트
- [ ] `InputPrompt` 타입: 에디터박스 입력 후 `GetInputText()` 정확
- [ ] `Info` 타입: `AutoCloseSeconds` 후 자동 닫힘
- [ ] RichText 마크업 정상 렌더링

### 3단계: UExToastWidget 구현

**작업 내용:**
1. `UExToastWidget` 클래스 작성 (`UCommonUserWidget` 상속)
2. `InitToast(FExToastDescriptor)` — 메시지 설정, 프로그레스바 모드 분기, 타이머 시작
3. `TickCountdown()` — 자동 감소 로직
4. `SetProgress()` / `CloseToast()` — 수동 제어 인터페이스
5. `FOnToastClosed` 델리게이트 — `CloseToast()` 내부에서 서브시스템에 닫힘 알림

**검증:**
- [ ] `ShowToast()` → ToastContainer에 추가 → Duration 후 자동 제거
- [ ] 자동 제거 시 `ActiveToasts` 배열에서도 해당 Toast 제거됨 (HandleToastClosed 동작 확인)
- [ ] `ShowTimedToast()` → 프로그레스바 표시, 시간에 따라 줄어듦
- [ ] `ShowLoadingToast()` → 프로그레스바 0% 대기, `SetProgress()` 반응
- [ ] Toast 3개 초과 시 가장 오래된 것 자동 제거, 새 Toast 정상 표시
- [ ] Toast 3개 자연 만료 후 새 Toast 정상 표시 (큐 갱신 검증)
- [ ] RichText 마크업 정상 동작

### 4단계: UIManagerSubsystem 확장

**작업 내용:**
1. 팝업 편의 함수 추가 (`ShowInfo`, `ShowAcknowledge`, `ShowConfirm`, `ShowInputPrompt`, `ShowPopup`)
2. Toast 편의 함수 추가 (`ShowToast`, `ShowTimedToast`, `ShowLoadingToast`, `ShowToastFromDescriptor`)
3. `RegisterToastContainer()` 추가
4. Toast 큐 관리: `TWeakObjectPtr` 기반 `ActiveToasts`, `HandleToastClosed` 콜백
5. `RegisterToastContainer(nullptr)` 호출로 레벨 전환 시 안전 해제

**검증:**
- [ ] C++: `ShowConfirm(Title, Body, WeakLambda)` → 팝업 → 버튼 클릭 → 람다 실행
- [ ] BP: `ShowConfirmBP` → OnPopupResult 바인딩 → 이벤트 수신
- [ ] Toast 편의 함수 4종 정상 동작
- [ ] `RegisterToastContainer()` 미호출 시 경고 로그 + nullptr 반환
- [ ] 레벨 전환 후 ToastContainer nullptr 해제, 재등록 시 정상 동작

### 5단계: BP 에셋 생성 (수동 에디터 단계)

**사용자 작업:**
1. `DT_ExRichTextStyles` DataTable 생성 및 스타일 행 추가
2. `WBP_ExPopupWidget` 블루프린트 생성:
   - 부모 클래스: `UExPopupWidget`
   - BindWidget: `Text_Title`, `Text_Body`, `Panel_Buttons`, `Panel_Input`, `EditBox_Input` 배치
   - `TextStyleSet`에 `DT_ExRichTextStyles` 할당
   - 시각 레이아웃 및 애니메이션 설계 (BP 자유)
3. `WBP_ExToastWidget` 블루프린트 생성:
   - 부모 클래스: `UExToastWidget`
   - BindWidget: `Text_Message`, `ProgressBar_Timer` 배치
   - `TextStyleSet`에 `DT_ExRichTextStyles` 할당
4. HUD Layout BP에 Toast 컨테이너 추가:
   - `VerticalBox` 배치 → 이름: `ToastContainer`
   - 앵커: 상단 중앙 또는 하단 중앙
5. HUD Layout `NativeConstruct`에서 `UIManagerSubsystem->RegisterToastContainer(ToastContainer)` 호출

---

## 하위 플러그인 확장 가이드

### 팝업 사용 (ExRunnerPlay 등)

```cpp
// CreateWeakLambda 필수
UIMgr->ShowConfirm(
    LOCTEXT("ReviveTitle", "부활"),
    LOCTEXT("ReviveBody", "<Emphasis>100골드</>를 소모하여 부활하시겠습니까?"),
    FOnExPopupResultNative::CreateWeakLambda(this, [this](EExModalResult Result, const FText&)
    {
        if (Result == EExModalResult::Confirmed) { Revive(); }
    })
);
```

### 커스텀 팝업 서브클래스 (v2.0 이후)

기본 Descriptor로 커버되지 않는 특수 팝업이 필요할 때:

1. `UExPopupWidget` 상속 → 하위 플러그인에 C++ 클래스 생성
2. 추가 BindWidget 정의 (이미지 슬롯, 3D 뷰포트 등)
3. `InitFromDescriptor()` 호출 후 추가 초기화
4. `UIManagerSubsystem::ShowPopup()`의 WidgetClass 오버로드 또는 `PushModal()` 사용

### Toast 확장

`UExToastWidget` 상속으로 커스텀 Toast 생성 가능. `UIManagerSubsystem`의 `ToastWidgetClass` 변경으로 프로젝트 전체 Toast 외관 교체 가능.

---

## 제약 사항 (UI 아키텍처 v2.1 규칙 준수)

1. **`SetInputMode` 절대 금지** — 입력 모드는 `GetDesiredInputConfig()` 오버라이드로만 관리
2. **`ActivateWidget()` 수동 호출 금지** — `MenuStack->AddWidget()`이 자동 활성화
3. **서버에서 UI 조작 금지** — 팝업은 로컬 클라이언트에서만 생성/조작
4. **`LOCTEXT` / `NSLOCTEXT` 사용** — `FText::FromString()` 하드코딩 금지, 현지화 대비
5. **ExCore → ExRunnerPlay 참조 금지** — 팝업 시스템은 ExCore에만 배치
6. **애니메이션은 BP 위임** — `BP_OnPopupShown()` / `BP_OnPopupHidden()` / `BP_OnToastShown()` / `BP_OnToastHidden()` BlueprintImplementableEvent로 BP에서 구현
7. **Native 델리게이트 바인딩 시 `CreateWeakLambda` 필수** — `FOnExPopupResultNative`에 람다를 바인딩할 때, 호출자(`this`)가 팝업 닫힘 시점에 파괴되어 있을 수 있다. 반드시 `CreateWeakLambda(this, [...]{...})`를 사용하여 호출자 수명이 끝났으면 콜백을 자동 무시하도록 한다. `CreateLambda`로 `this`를 직접 캡처하면 Dangling Pointer 크래시 위험.
8. **Toast `ActiveToasts` 배열은 `TWeakObjectPtr` 사용** — Toast 위젯의 GC 소유권은 HUD(UPanelWidget)에 있다. 서브시스템은 추적만 하므로 `UPROPERTY()` 없이 `TWeakObjectPtr`로 약참조한다. `UPROPERTY()` + `TWeakObjectPtr` 조합은 GC 메커니즘과 충돌하므로 사용 금지.
