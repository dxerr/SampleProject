# ExCore 아키텍처 핵심 지침서

> 출처: `Md/Architecture/ExCore/` 9개 문서 분석  
> 엔진: UE 5.7.3 | 작성: 2026-05-11

---

## 1. 모듈 계층 원칙

- `ExCore`는 **범용 프레임워크**. 특정 장르(러너/배틀) 로직 포함 금지.
- `ExCore`는 `ExRunnerPlay`를 절대 참조하지 않는다. **의존성 방향은 Feature → Core 단방향.**
- 기능 배치 판단: *"다른 장르에서도 쓸 수 있나?"* → Yes: ExCore / No: ExRunnerPlay

---

## 2. GameFeature 에셋 쿠킹

- 맵/에셋 쿠킹 설정은 **`DefaultGame.ini`가 아닌 `GameFeatureData` 에셋 내 Asset Manager**에서 관리.
- `UWorld(Map)` 타입 직접 스캔 시 패키징 크래시 발생 → **`UExFeatureAssetManifest`(PrimaryDataAsset) 경유** 필수.
- Cook Rule은 반드시 **`Always Cook`** 으로 설정.

---

## 3. GameplayTag 이벤트 시스템

- 시스템 간 통신은 직접 참조 없이 **`UExGameplayEventSubsystem`(UWorldSubsystem)** 을 경유.
- 새 태그 추가: `ExGameplayTags.h` 선언 → `ExGameplayTags.cpp` 정의 (순서 필수).
- **현재 이벤트는 로컬 전용**. 네트워크 복제가 필요하면 별도 RPC 구현.

---

## 4. DataCenter 시스템

### DataAsset 유형 판단 기준
| 조건 | 베이스 클래스 |
|---|---|
| 전역 수치/설정, 인스턴스 1개 | `UExConfigDataAsset` |
| 개체 자체 정의, 인스턴스 N개 | `UExDefinitionDataAsset` (DefinitionTag 필수) |
| Definition들의 조합/룰 | `UExPresetDataAsset` (PresetTag 필수) |

### 필수 규칙
- DataCenter 조회는 **`BeginPlay`에서 1회만**, `TWeakObjectPtr`로 캐싱.
- **Tick에서 매 프레임 DataCenter 직접 조회 금지.**
- **Raw Pointer 캐싱 금지** (GameFeature 비활성화 시 Stale 참조 위험).
- 서브클래스에서 `IsDataValid` 오버라이드 시 반드시 `Super::IsDataValid()` 먼저 호출.
- 에셋 명명: Config → `DA_ExConfig_[모듈]` / Definition → `DA_Ex[타입]_[이름]` / Preset → `DA_ExPreset_[용도]`

---

## 5. 입력 시스템 (Unified Input)

- 하드웨어 입력(Enhanced Input)과 UI 터치 입력(MVVM)은 **`UExInputComponentBase` 파생 클래스**에서 동일한 델리게이트로 통합.
- 캐릭터 BP는 `IA_Jump` 등 Enhanced Input 노드를 **직접 사용하지 않음**. 반드시 InputComponent의 `On Xxx Requested` 델리게이트만 구독.
- 새 입력 추가 시: C++ 컴포넌트에 델리게이트 선언 → `InitializeInputBindings()` 오버라이드 → UI ViewModel에서 동일 경로 호출.

---

## 6. 멀티플레이어 Flow 아키텍처

### 상태 계층 (두 계층 혼동 금지)
- **앱 레벨**: `UExGameFlowSubsystem`(GameInstanceSubsystem) 관할 → `Boot → Auth → Lobby → InGame`
- **매치 레벨**: `AExGameStateBase`(복제됨) 관할 → `WaitingForPlayers → Countdown → Playing → PostMatch`

### 절대 금지
- 클라이언트에서 `GetWorld()->GetAuthGameMode()` 호출 (항상 `nullptr` → 크래시).
- 서버가 Widget 생성/조작하는 행위.
- `ClientRPC_UpdateUI()` 형태의 UI 직접 갱신 RPC.
- `UWorldSubsystem`에서 `HasAuthority()` 사용 → `GetWorld()->GetNetMode()` 사용.

### 필수 패턴
- `UPROPERTY(Replicated)` 추가 시 반드시 `GetLifetimeReplicatedProps`에 등록.
- **JIP(난입) 가드**: `GameStateBase::BeginPlay`에서 현재 상태로 델리게이트 강제 Broadcast 1회.
- Travel 실행 권한은 `AExGameModeBase`만 보유. `GameFlowSubsystem`은 `OnRequestTravel` 델리게이트 발행만 담당.
- 데이터 흐름: `GameMode → GameState(복제) → OnRep → 로컬 델리게이트 → UI 구독`.

---

## 7. UI 시스템 (CommonUI + MVVM)

### 위젯 3분류 (반드시 준수)
| 분류 | 베이스 클래스 | 용도 |
|---|---|---|
| HUD Layout | `UExHUDLayoutWidget` | 항상 존재, GameStack/MenuStack 소유 |
| 창모드형 | `UExWindowWidget` | 인벤토리, 설정 등 대형 패널 |
| 팝업형 | `UExModalWidget` | 확인창, 경고, 보상 팝업 |

### 절대 금지
- `SetInputModeGameOnly/UIOnly/GameAndUI()` 사용 금지 (CommonUI 입력 라우팅 파괴).
- `ActivateWidget()` 수동 호출 금지 (이중 활성화 → 입력 라우팅 파괴).
- `UUserWidget`을 메뉴/창/팝업의 베이스로 사용 금지. 반드시 `UCommonActivatableWidget` 파생 사용.
- UI 업데이트에 `Event Tick` 사용 금지. **반드시 FieldNotify 바인딩만 사용.**

### 필수 설정
- `Game Viewport Client Class` = **`CommonGameViewportClient`** (미설정 시 입력 라우팅 전혀 동작 안 함).
- `CommonButtonBase` 내 아이콘 위젯 이름은 반드시 **`InputActionWidget`** (다른 이름 사용 시 게임패드 아이콘 미표시).

### MVVM 필수 규칙
- View Bindings **Target으로 사용할 변수**는 반드시 `BlueprintReadWrite` 선언 (없으면 `"not writable at runtime"` 에러).
- 파생 계산값은 `UFUNCTION(BlueprintPure, FieldNotify)` 선언 → Source 바인딩만 가능.
- Setter에서 `UE_MVVM_SET_PROPERTY_VALUE` 매크로 필수. 파생값 변경 시 `UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED` 추가.
- ViewModel 접근 시점: `On Initialized` ❌ → **`On Activated` ✅** (Create Instance 방식은 이 시점에 생성).

---

## 8. 사운드 시스템

- `UExMusicManagerSubsystem`(UWorldSubsystem)이 BGM 전담. ExRunnerGameMode가 직접 제어.
- Phase별 레이어 볼륨은 `UExMusicPhaseDataAsset`으로 **에디터에서 데이터 드리븐** 설정.
- MetaSound Graph Input 이름 ↔ C++ `SetFloatParameter()` 이름 **반드시 1:1 일치**.
- 비트/마디 이벤트는 `ExGameplayEventSubsystem`으로 브로드캐스트 → 느슨한 결합 유지.

---

## 9. Mover 입력 시스템

- 기존 CMC 아닌 `CharacterMoverComponent` 사용. 입력 인터페이스는 `IMoverInputProducerInterface::ProduceInput`.
- `gather_input_from_all_input_producer_components = True` → 인터페이스를 구현한 컴포넌트는 **자동 감지**됨.
- 러너 전진 입력은 `UExRunnerMovementComponent`가 `IMoverInputProducerInterface`를 구현하여 `ProduceInput`에서 강제 주입.
- `SetDirectionalInput` 사용 권장 (물리 기반). `SetVelocityInput`은 직접 속도 지정 시만 사용.
