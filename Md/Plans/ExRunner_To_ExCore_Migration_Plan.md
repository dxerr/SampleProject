# ExRunner → ExCore 이관 및 공용 시스템화 계획

## 개요
새로운 GameFeature 기반 게임 모드를 제작하기 위해, 기존 `ExRunnerPlay` 모듈에 종속되어 있던 시스템 중 범용적으로 활용 가능한 기능들을 찾아내어 `ExCore` 모듈로 이관 및 추상화하는 계획입니다. 이를 통해 향후 어떤 장르의 GameFeature를 추가하더라도 `ExCore`의 뼈대를 그대로 재사용할 수 있게 됩니다.

## 1. 이관 및 일반화 대상 시스템 목록

### 1.1 Match & Lobby UI 시스템 (MVVM)
- **[이관 대상]** `ExLobbyMatchViewModel`
  - **이유**: EOS의 QuickMatch API를 래핑하고, 팝업 및 로비 상태를 제어하는 로직은 어느 게임 모드에서나 100% 재사용 가능합니다.
  - **계획**: `ExCore/UI/ViewModels`로 이동하여 `ExLobbyViewModelBase` 형태로 추상화합니다.
- **[이관 대상]** `ExRunnerMatchViewModel`
  - **이유**: GameState의 `MatchPhase`(Waiting -> Playing -> PostMatch)를 감지하여 HUD 스위처(Switcher) 위젯을 전환하는 기능은 모든 매치 기반 게임에 동일하게 필요합니다.
  - **계획**: `ExMatchViewModelBase`로 이름 변경 및 ExCore로 이관합니다.

### 1.2 Rule & Objective 시스템 (규칙 관리)
- **[이관 대상]** `ExRunnerRuleManagerComponent` 및 `ExRunnerRuleBase`
  - **이유**: 어떤 게임이든 승리/패배/종료 조건(Rule)을 컴포넌트 단위로 조립하여 사용하는 아키텍처는 매우 강력하고 범용적입니다.
  - **계획**:
    - `ExRuleManagerComponent`, `ExRuleBase`로 이름 변경하여 `ExCore/Rules`에 배치합니다.
    - 기존의 `InGameMode` 캐싱 부분을 `ExGameModeBase`로 변경하여 결합도를 낮춥니다.
    - **[설계 피드백 반영]**: 통일성을 위해 기존 방식과 동일하게 DataAsset을 ManagerComponent에 꽂아 넣는 방식으로 유지합니다.

### 1.3 Buff & Status Effect 시스템 (상태 변화)
- **[이관 대상]** `ExRunnerBuffComponent` 및 `FExBuffDefinition`
  - **이유**: 일정 시간 유지 후 소멸되는 버프/디버프 관리(타이머, 우선순위 처리, UI 연동을 위한 잔여 시간 폴링)는 범용적인 필수 기능입니다.
  - **계획**:
    - `ExBuffComponent`로 변경하여 `ExCore/Components`로 이관합니다.
    - **[설계 피드백 반영]**: 기존 방식처럼 인터페이스 함수 호출 방식을 유지하며 모듈을 분리합니다.

### 1.4 Score & Stat 시스템
- **[이관 대상]** `ExRunnerStatComponent`
  - **이유**: 점수, 재화(코인) 획득, 그리고 상태값 보관은 어느 게임에나 존재합니다.
  - **계획**:
    - **[설계 피드백 반영]**: 체력(HP), 마나(MP) 등 RPG 요소를 고려한 확장 가능한 범용 스탯 시스템(`ExStatComponent`)을 염두에 두고 설계 및 이관합니다.

### 1.5 Rhythm & Music 시스템
- **[이관 대상]** `ExBeatSyncComponent`
  - **이유**: 음악의 BPM에 맞춰 이벤트를 발생시키는 기능은 러너 장르를 떠나 범용적으로 쓰일 수 있습니다.
  - **계획**: 그대로 `ExCore/Components`로 이동하여 공용 시스템으로 격상합니다.

---

## 2. 작업 진행 순서 (Phase)

- **Phase 1**: `ExLobbyMatchViewModel`, `ExMatchViewModel` 등 독립성이 강한 UI 뷰모델 클래스 이관 및 이름 변경
- **Phase 2**: `ExBeatSyncComponent`, `ExBuffComponent`의 의존성 제거 및 이관
- **Phase 3**: Rule 시스템(`ExRuleManagerComponent`) 추상화 및 ExCore 이관
- **Phase 4**: 스탯 시스템(`ExStatComponent`) RPG 확장성 구조로 리팩토링 및 이관
- **Phase 5**: 기존 `ExRunnerPlay`의 게임모드 및 폰에서 이관된 ExCore 컴포넌트를 사용하도록 리팩토링 및 빌드 검증

