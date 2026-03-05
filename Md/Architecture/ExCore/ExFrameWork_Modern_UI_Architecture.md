# ExFrameWork: 모던 UI 시스템 아키텍처 설계서

> **버전:** v2.1  
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

이 UI 시스템은 멀티플레이어 Flow 아키텍처에서 정의한 레이어 구조를 따른다. 모든 UI 베이스 클래스는 **ExCore 플러그인**에 배치한다.
특히, 전역 상태 제어(`UExGameFlowSubsystem`), 매치 페이즈(`AExGameStateBase`), 외부 통신(`UExBackendCommunicationSubsystem`)과 UI 간의 결합도를 낮추고 수동적 이벤트 수신 위주로 동작하게 한다.

### 1.3 UI 데이터 흐름 (멀티플레이어 환경)

UI는 서버 데이터를 직접 조작하거나 직접 Tick으로 검사하지 않는다. 반드시 아래 경로를 따른다:

```
서버(GameMode) → 복제(Replicate) → GameState/PlayerState → OnRep → 로컬 델리게이트
→ ViewModel(FieldNotify) → Widget 자동 업데이트

예시 1: 매치 상태 변경 (난입 유저 지원 포함)
  GameMode::SetMatchPhase() 
    → GameState::CurrentMatchPhase (복제)
      → OnRep_MatchPhase() (난입 시 로컬에서 강제 Broadcast 됨 보장)
        → OnMatchPhaseChanged 델리게이트 Broadcast
          → UExMatchViewModel::SetMatchPhase()
            → FieldNotify → HUD Widget 자동 업데이트

예시 2: 서버 인증 대기 UI 로직
  클라이언트 UI 버튼 입력
    → UExBackendCommunicationSubsystem::RequestLogin()
      → HTTP 통신 대기 (진행 스피너 Popup 노출)
        → OnLoginSuccess/Failed 델리게이트 수신
          → Popup 종료 및 매칭 로비 창(PushWindow)으로 전환
```

**핵심 규칙:** 서버가 위젯을 생성하거나 조작해서는 절대 안 된다. UI는 복제된 데이터의 변경 알림이나 로컬 매니저의 이벤트에 의해서만 구동된다.

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

---

## 2. 서버 데이터 무결성 및 중앙 집중화 관리 (Single Source of Truth)

주인님께서 제안하신 **"서버 제어 및 UI 갱신이 필요한 모든 데이터를 한 곳에 구조화하여 선언하고 관리한다"**는 아이디어는 최신 아키텍처 설계에서 가장 중요한 **단일 진실 공급원(Single Source of Truth, SSOT)** 원칙에 정확히 부합하며, 매우 훌륭한 접근입니다. 동기화 이슈나 데이터 중복을 원천 차단하기 위해 이 원칙을 UI 설계의 근본 아키텍처로 추가 확립합니다.

### 2.1 "RPC 갱신 절대 금지, OnRep 필수" 원칙 (데이터 꼬임 방지)
- **위험성 (Anti-Pattern):** `Client_UpdateHP(int NewHP)`와 같은 클라이언트 RPC를 통해 UI 데이터를 갱신하면 절대 안 됩니다. 네트워크 지연(Lag)이나 패킷 유실로 인해 RPC 도착 순서가 꼬이면, UI에 표시되는 숫자와 실제 서버 메모리의 숫자가 불일치하는 심각한 무결성 파괴가 발생합니다.
- **아키텍처 강제:** UI 갱신이 필요한 모든 상태(State) 데이터는 오직 **서버에서 값을 변경**하고, `UPROPERTY(ReplicatedUsing=OnRep_...)`로 선언된 **리플리케이션 시스템(OnRep)만을 통해 클라이언트로 전파**되어야 합니다.

### 2.2 도메인별 데이터 중앙 집중화 체계
단 하나의 거대한 전역 구조체(Struct)에 모든 걸 몰아넣는 것은 언리얼 네트워크 대역폭 낭비(변수 하나만 바뀌어도 구조체 전체 검사)를 초래할 수 있습니다. 따라서 **데이터를 목적(Domain)별 전용 Component로 철저히 집약**시키는 아키텍처를 제안합니다.

| 데이터 도메인 | 중앙 관리 위치 (구조화 포인트) | 통제 방식 | UI 갱신 바인딩 경로 |
|---|---|---|---|
| **매치/글로벌** | `AExGameStateBase` (게임 페이즈, 타이머 등) | 서버 권한(`HasAuthority()`)으로 직접 변경 | `OnRep_MatchPhase` → ViewModel 반영 |
| **개인 통계** | `AExPlayerStateBase` (스코어, 킬/데스, 핑) | 서버 로직 내장 함수 호출 | 엔진 기본 `OnRep_Score` 오버라이딩 활용 |
| **전투 스탯** | `UExStatComponent` (HP, MP, 속도, 스태미나 등) | 한 곳의 구조체/컴포넌트에서만 스테이터스 관리 | 개별 `OnRep_Health` 등 → ViewModel 바인딩 |
| **인벤토리** | `UExInventoryComponent` (아이템 리스트 등 배열형) | `FFastArraySerializer` 방식 사용 (고도 최적화) | 변경된 아이템 델리게이트 발송 |

### 2.3 데이터와 UI 로직의 완전한 분리 (Decoupling)
주인님께서 지적하신 **"데이터와 로직의 분리"**는 이 아키텍처(MVVM 패턴)의 핵심 철학이자 가장 큰 장점입니다. UI가 데이터를 소유하는 것이 절대 아닙니다.
- **순수 데이터 컴포넌트 (Model):** HP, MP 등의 원본 데이터는 UI 클래스에 존재하지 않습니다. 오직 캐릭터나 플레이어에 부착된 `UExStatComponent` (또는 GAS의 `AttributeSet`) 내부의 구조체에 독립적으로 존재합니다.
- **자유로운 다중 참조:** 캐릭터의 애니메이션 블루프린트, 데미지 피격 로직, FX 시스템 등 어떠한 게임 로직 파트라도 UI의 존재 여부를 전혀 모른 채, 오직 `UExStatComponent`의 HP 값만 순수하게 가져다 쓸 수 있도록 설계되어 있습니다.
- **UI의 일방적 관찰 (View & ViewModel):** 상태 데이터 컴포넌트(`UExStatComponent`)는 누가 자신을 쳐다보는지 모릅니다. 데이터가 변하면 그저 "내 체력이 x로 변했다"라고 허공에 외치면(`Broadcast`), UI를 담당하는 ViewModel만이 그 소리를 듣고 화면의 게이지바를 갱신합니다. 이를 통해 게임 로직 코드가 UI 코드에 의존(Dependency)하게 되는 스파게티 코드를 완벽하게 분리, 원천 차단합니다.

### 2.4 무결성 검증 파이프라인 (JIP 및 재연결 대응)
어떤 클라이언트가 중도 난입(Join-in-Progress)하거나 네트워크 연결이 불안정해도 무조건 데이터 무결성이 보장되는 파이프라인입니다.
1. **[행동 요청]** 클라이언트: `ServerRPC_UseItem()` 호출 (이때 클라이언트 UI는 어떠한 선반영도 하지 않음)
2. **[서버 검증]** 중앙 집중화된 Component(또는 GameMode) 내부에서만 `HasAuthority()` 검사 및 아이템 소지 여부 확인
3. **[서버 반영]** 검증 통과 시 `CurrentHP += 50;` 형태로 서버 단일 변수만 업데이트
4. **[엔진 자동화]** 틱 파이프라인 끝에서 엔진이 변경점을 수집하고 네트워크를 통해 클라이언트로 안전하게 Replication 배포
5. **[수신 및 자동 갱신]** 클라이언트에서 변경점 수신 시 `OnRep_HP` 발동 → 브로드캐스트 → `ViewModel` 갱신 → UI 자동 업데이트

> **💡 추가 강력 제안 (언리얼 공식 프레임워크인 GAS 검토):**
> 만약 HP, MP, 달리기 스태미나, 버프/디버프 등 관리해야 할 전투용 스탯 수치가 방대해진다면, 커스텀 구조체를 만들기보다는 언리얼 공식 시스템인 **Gameplay Ability System(GAS)의 `UAttributeSet`** 사용을 면밀히 리서치 및 도입해 보시는 것을 가장 강력히 추천합니다. GAS는 동기화, JIP 처리, 클라이언트 단의 예측(Prediction), 네트워킹 최적화를 엔진 코어가 완벽하게 책임지며, 주인님이 원하시는 **"한 곳에 선언하고 동기화 이슈를 신경 쓰지 않아도 되는 이상적인 구조"**를 이미 완성형으로 제공합니다.

---

## 3. 엄격한 제약 사항 (모든 단계에 적용)

### 3.1 CommonUI 필수 규칙

**입력 모드 관리:**
- `SetInputModeGameOnly()`, `SetInputModeUIOnly()`, `SetInputModeGameAndUI()` 를 **절대 사용하지 마라**. CommonUI 환경에서 이 함수들은 입력 라우팅 시스템을 완전히 깨뜨린다.
- 대신 각 `UCommonActivatableWidget` 파생 클래스에서 `GetDesiredInputConfig()` 함수를 오버라이드하여 해당 위젯이 활성화될 때 적용할 입력 설정을 선언적으로 반환한다.

**위젯 활성화:**
- `ActivateWidget()`을 수동으로 호출하지 마라. `UCommonActivatableWidgetContainerBase::AddWidget()`(= `PushWidget`)이 위젯을 스택에 추가하면 자동으로 활성화된다.

**GameViewportClient:**
- 프로젝트 설정에서 `GameViewportClientClass`를 반드시 `CommonGameViewportClient`로 변경해야 한다.

### 3.2 MVVM 필수 규칙

- UI 업데이트를 위해 `Event Tick`을 절대 사용하지 마라.
- ViewModel 프로퍼티는 `BlueprintReadOnly, FieldNotify`로 선언한다.
- 강제 초기화(JIP) 처리를 대비하여, ViewModel 연결 시점(`NativeConstruct` 등)에서 데이터가 이미 바뀌어있을 가능성을 염두에 둔다.

---

## 4. 위젯 3분류 체계 및 스택 구조 다이어그램

| 분류 | 베이스 클래스명 | 스택 관계 | 입력 모드 |
|---|---|---|---|
| HUD Layout | `UExHUDLayoutWidget` | 스택의 **소유자** | Game |
| 창모드형 | `UExWindowWidget` | MenuStack에 **Push** | Menu |
| 팝업형 | `UExModalWidget` | MenuStack에 **Push (최상단)** | Modal: Menu / Modeless: All |

```
UExHUDLayoutWidget (항상 활성)
│
├── GameStack (UCommonActivatableWidgetStack, Z: 하위)
└── MenuStack (UCommonActivatableWidgetStack, Z: 상위)
    │
    ├── UExWindowWidget 파생 (인벤토리 등) ← Push/Pop
    └── UExModalWidget 파생 (확인창 등)  ← Push/Pop (최상단)
```

---

## 5. 1단계 구현: 환경 및 모듈 설정 (먼저 진행)

### 5.1 Build.cs 모듈 추가

```csharp
// ExCore.Build.cs
PublicDependencyModuleNames.AddRange(new string[]
{
    "UMG",
    "CommonUI",
    "CommonInput",        // CommonUI 입력 시스템에 필수
    "ModelViewViewModel",
});
```

### 5.2 매뉴얼 에디터 설정 (필수)
1. **플러그인 활성화:** `Common UI Plugin`, `UI Model View View Model`
2. **Game Viewport Client Class:** `CommonGameViewportClient` 로 변경
3. **Enhanced Input 연동:** `Common Input Settings`에서 Enable
4. **Data Assets 생성:** `CommonInputActionDataBase` 행 추가, `CommonUIInputData` 세팅 및 플랫폼 Input 할당 (`Generic` 게임패드 네이밍 필수).

---

## 6. 2단계 구현: UI 매니저 서브시스템 (`UExUIManagerSubsystem`)

* **부모 클래스:** `ULocalPlayerSubsystem` (분할 화면 시 플레이어별 UI 독립성 보장)
* **주요 역할:** `UExHUDLayoutWidget`으로부터 `GameStack`, `MenuStack`의 포인터를 등록(`RegisterStacks`)받고, 어디서든 `PushWindow()`, `PushModal()`, `PopMenu()` 를 호출할 수 있게 해주는 UI 관문.
* **주의사항:** 내부에서 절대 수동으로 `ActivateWidget`을 부르거나 비상식적인 `InputMode` 변경을 수행해서는 안 됨.

---

## 7. 3단계 구현: 3분류 베이스 및 버튼 구현

1. **`UExHUDLayoutWidget`:**
   - 뷰포트에 추가되는 베이스 위젯.
   - `NativeOnActivated`에서 `RegisterStacksToManager`를 호출해 UI 매니저를 깨움.
   - JIP로 들어온 `AExGameStateBase`의 초기값 브로드캐스트를 받아낼 수 있도록 생명주기를 구성해야 함.

2. **`UExWindowWidget`:**
   - 대형 인벤토리나 설정창용. `bAllowGameInputWhileOpen`를 두어 기본적으로 `Menu` 입력 전용 사용.

3. **`UExModalWidget`:**
   - 팝업/확인용 레이아웃. 
   - HTTP 통신(`UExBackendCommunicationSubsystem`) 대기 시, 응답 델리게이트(`OnLoginSuccess`/`Failed`)를 인자로 받아 통과되거나 에러를 띄워주는 통합 Result 델리게이트 구조(`EExModalResult`) 필수 포함.

4. **`UExBaseButtonWidget`:**
   - `CommonActionWidget`의 이름을 강력하게 **`InputActionWidget`**으로 강구하여 PC/패드 전환 아이콘이 깨지지 않게 적용.

---

## 8. 4단계 구현: MVVM ViewModel 베이스 (`UExPlayerStatsViewModel` 등)

* `UMVVMViewModelBase` 상속 구조 적용.
* `PlayerState`나 서버 통계(`ExGameStateBase`)의 데이터 변동을 수신할 프로퍼티들을 `BlueprintReadOnly, FieldNotify` 셋업.
* UI 단에서는 View bindings 패널로 안전하게 `Create Instance` 하여 메모리 릭 없게 처리.

---

## 9. 전체 연동 정리 및 시퀀스

1. `ExFlowTags::Flow_Lobby` 시작 시, HUD 레이아웃 `UExLobbyHUDLayout` 생성 로드.
2. 로컬 플레이어 UIManager가 스택 등록.
3. 유저가 '게임진입' 선택 시 `ExGameFlowSubsystem`로 RequestTravel.
4. `Match_WaitingForPlayers` 단계 시 `ExMatchViewModel`에 의해 상단 UI "플레이어 대기 중" 글씨 자동 세팅.
5. 유저 JIP(난입) 입장 시, `GameState->BeginPlay`에서 `GetLocalRole() < ROLE_Authority` 확인하여 강제 Broadcast.
6. MVP 패턴에 의해 별도의 Update 로직 없이 UI 자동 동기화. 

---
**개선 및 추가 설계 확정:**
1. Flow 아키텍처에 추가되었던 JIP 대응(GameState의 수동 Broadcast) 고려사항 MVVM 명시.
2. HTTP BackendSubsystem과의 결합부(Popup 연계) 데이터 플로우 명시.
3. 모듈 의존성 보강 완료.
