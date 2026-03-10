# ExFrameWork: 경험(Experience) 시스템 제작 및 설정 가이드

> **버전:** v1.0  
> **대상 엔진:** Unreal Engine 5  
> **프로젝트:** ExFrameWork  

---

## 1. 개요
ExFrameWork의 **경험(Experience) 시스템**은 Lyra 스타일에 착안하여, 맵 구동 시 초기화되어야 할 UI, 활성 시스템 위젯 등을 하드코딩 없이 **데이터 주도(Data-Driven)** 방식으로 동적 할당하기 위해 만들어졌습니다.
특정 GameMode에 종속된 맵을 로드했을 때, 서버가 지정한 `UExExperienceDefinition` 데이터가 클라이언트로 복제되며 클라이언트는 이에 맞춰 필요한 UI(HUD)와 추가 요소들을 메모리에 로드한 뒤 게임플레이(Playing)를 준비합니다.

## 2. 핵심 구성 요소
- **`UExExperienceDefinition` (DataAsset):** 인게임 맵 혹은 로비 화면에서 어떠한 형태의 기본 HUD와 스택 위젯들을 켜야 하는지에 대한 데이터 구조체입니다.
- **`UExExperienceManagerComponent`:** GameStateBase에 부착되어 실질적인 복제 이벤트를 받고 UI 생성 명령을 수행하는 로딩 매니저입니다.
- **`AExGameModeBase`:** 맵 로딩 시 어떤 DataAsset을 뿌릴 것인지 결정(`DefaultExperience`)하고, 클라이언트의 로딩 완료 신호를 받아 맵을 시작 상태(`Match_Playing`)로 넘깁니다.

---

## 3. Experience 데이터 에셋(DataAsset) 제작 방법

새로운 맵이나 새로운 UI 구성을 가진 모드를 만들 때 다음 순서대로 작업을 진행합니다.

### 3.1. UExExperienceDefinition 생성
1. 언리얼 에디터 콘텐츠 브라우저에서 우클릭 -> **Miscellaneous (기타)** -> **Data Asset (데이터 에셋)** 을 선택합니다.
2. 부모 클래스로 **`ExExperienceDefinition`** 을 선택합니다.
3. 생성된 에셋의 이름을 규칙에 맞게 짓습니다. (예: `DA_ExRunnerExperience`, `DA_ExLobbyExperience`)

### 3.2. 데이터 세팅
만들어진 데이터 에셋 더블 클릭하여 디테일 창을 엽니다.
- **`Default HUD Layout`:** 이 모드에서 띄워둘 메인 UI 레이아웃 베이스. (예: `WBP_ExRunnerHUDLayout`)
  - 여기에는 캔버스 패널 위주의 베이스 판과, `CommonAnimatedSwitcher` 같은 트랜지션 관리 위젯이 들어갑니다.
- **`Extra Widgets to Load` (선택):** 기본 HUD 외에 활성화 스택에 밀어넣거나 백그라운드에서 로딩해야 할 추가 `UCommonActivatableWidget` 리스트입니다. (현재는 빈 배열로 두어도 무방합니다)

---

## 4. GameMode에 Experience 적용하기

제작한 DataAsset을 실제 맵 구동 시 사용하려면 해당 맵을 관장하는 GameMode 블루프린트에 등록해야 합니다.

### 4.1 GameMode 설정
1. 대상 맵에 적용된 **GameMode 블루프린트** (예: `BP_ExRunnerGameMode`) 를 엽니다.
2. 디테일 패널 검색창에 **Experience** 를 검색합니다.
3. `Ex Match | Experience` 카테고리 아래 있는 **`Default Experience`** 속성에 조금 전 만든 DataAsset (예: `DA_ExRunnerExperience`) 을 할당합니다.

### 4.2 매치 자동 시작 설정 (bAutoStartOnReady)
단독 맵 테스트 시, 로딩이 완료된 즉시 플레이 상태(Playing)로 넘어가게 만들고 싶다면:
- GameMode 디테일에서 **`Auto Start On Ready`** 속성에 체크합니다.
- (C++ 클래스 생성자에서 `bAutoStartOnReady = true;`로 설정되어 있다면 자동으로 켜져 있습니다.)

---

## 5. 실행 흐름 (내부 작동 원리)

1. **(서버) 맵 시작:** `ExGameModeBase::InitGame()` 실행 시 `DefaultExperience` 에셋을 `ExGameStateBase` 내부의 `ExperienceManagerComponent` 변수(`CurrentExperience`)에 주입합니다.
2. **(서버 -> 클라) 리플리케이트:** 새로운 Experience 데이터가 클라이언트로 네트워크 복제됩니다.
3. **(클라이언트) UI 로딩:** `OnRep_CurrentExperience()`가 발동하며, 클라이언트는 `DefaultHUDLayout` 클래스를 읽어들여 뷰포트에 띄웁니다.
4. **(클라이언트) 준비 완료:** 로딩이 끝나면 `ExPlayerControllerBase`가 서버에 `Server_NotifyReadyForMatch` RPC를 쏩니다.
5. **(서버) 게임 시작:** 모든 클라이언트 컨트롤러가 신호를 보낸 것을 확인(`CheckAndStartMatch`)하면, GameMode가 즉시 매치 단계를 `Match_Playing`으로 변경되어 본 게임이 시작됩니다.

---

## 6. 주의 사항
- `GameMode`의 **GameMode Override**를 올바로 지정하지 않으면, 기껏 만든 Experience 데이터가 적용되지 않습니다. (World Settings 주의)
- 클라이언트 UI가 로드되기 전에 조기에 게임 진행 코드가 실행되는 것을 막기 위해, 실제 게임(플레이어 움직임 가능 등) 로직은 **`GameState`의 `Match.Playing` 페이즈 전환 콜백(`OnMatchPhaseChanged`)** 이후에 시작하도록 설계해야 결함이 없습니다.
