# Bug Report: B PC 싱글 플레이 실행 시 EOS 로비 노출 및 DataCenter Config 누락으로 인한 A-B PC 간 매칭 실패 분석

> 작성일: 2026-05-20
> 대상 파일: [ExLobbyMatchViewModel.cpp](file:///c:/wz/ExFrameWork/Plugins/GameFeatures/ExRunnerPlay/Source/ExRunnerPlayRuntime/UI/ViewModels/ExLobbyMatchViewModel.cpp), [ExOnlineSubsystem.cpp](file:///c:/wz/ExFrameWork/Plugins/ExNetwork/Source/ExNetworkRuntime/Core/ExOnlineSubsystem.cpp)
> 상태: **원인 규명 완료 및 수정 계획 수립 (주인님 승인 대기 중)**

---

## 1. 이슈 개요 (Issue Summary)
- **현상**: A PC(에디터 PIE)와 B PC(Windows 패키징 빌드) 간의 멀티플레이 매칭 테스트 중, A PC가 B PC의 세션에 정상적으로 입장하지 못하고 최종 P2P 연결 타임아웃 및 `SE_ECONNREFUSED` 에러와 함께 연결이 종료되어 튕겨 나가는 현상이 발생하였습니다.
- **물증 (B PC 로그 `C:\wz\ExFrameWork.log` 분석 결과)**:
  1. 호스트 B PC가 멀티플레이가 아닌 **싱글 플레이(Single Play)** 모드로 게임을 시작하였음에도 불구하고, **EOS 상에 실제 공개 로비 세션을 생성**하였습니다.
  2. B PC는 1인 기준 정원(`ExpectedPlayerCount = 1`)이 충족되자마자 즉시 인게임 맵으로 `ServerTravel`을 돌고, 로딩 직후 EOS 로비를 즉각 파괴(`DestroyLobby`)하였습니다.
  3. A PC는 B PC가 싱글 플레이용으로 만든 방이 EOS에 노출되어 그 방에 잘못 참가하였고, B PC가 이미 방을 깨고 넘어갔기 때문에 P2P 접속 패킷을 날려도 응답을 받지 못해 타임아웃이 발생했습니다.
  4. 추가로, B PC 패키징 빌드 환경에서 `ExRunnerConfig` 에셋이 DataCenter에 등록되지 않아 `Ensure` 경고 크래시와 데이터 유실이 동반 발생하고 있었습니다.

---

## 2. 원인 분석 (Root Cause Analysis)

### 2.1 싱글 플레이 모드에서의 불필요한 EOS 로비 개설 및 외부 노출
- **동작 흐름**:
  1. B PC에서 싱글 플레이를 실행하면 `UExLobbyMatchViewModel::StartSinglePlay()`가 호출되어 `PendingConfig.bIsSinglePlay = true` 및 `ExpectedPlayerCount = 1`을 설정하고 `CachedOnlineSubsystem->FindQuickMatch(PendingConfig)`를 호출합니다.
  2. `FindQuickMatch`에서는 `Config.bIsSinglePlay`가 참이므로 검색(Searching)을 생략하고 즉시 로비 생성(Creating) 상태로 전이합니다.
  3. 이로 인해 `ExEOSLobbyProvider::CreateLobby`가 수행되어 **실제 EOS 플랫폼에 로비 세션을 등록**하게 되며, 이때 `bShouldAdvertise=1` 옵션이 켜져 외부 매칭 검색 풀에 이 "싱글 플레이 방"이 무방비로 노출됩니다.
- **매칭 오염 및 타임아웃의 인과관계**:
  - 멀티플레이 매칭을 찾던 클라이언트 A PC가 검색 중 B PC가 만든 싱글 플레이 방을 발견하고 `JoinLobby`를 수행합니다.
  - 호스트 B PC는 자신이 `ExpectedPlayerCount = 1`인 방의 호스트이므로, 본인이 들어가자마자 정원이 다 찼다고 판단하여 대기 팝업을 닫고 인게임 맵(`/ExRunnerPlay/Map/L_ExRunnerTest`)으로 `ServerTravel`을 실행해 버립니다.
  - 인게임 로딩 완료 직후 B PC는 `ServerTravel 후 로비 정리 실행` 루틴에 의해 `DestroyLobby`를 호출하여 로비를 공중분해시킵니다.
  - 뒤늦게 `ClientTravel`을 위해 P2P 시그널링(`com.epicgames.p2p.request_connection`)을 날리는 A PC는 이미 소멸한 로비와 파괴된 넷드라이버 컨텍스트 때문에 호스트로부터 어떠한 응답도 받지 못해 P2P 타임아웃(`SE_ECONNREFUSED`)으로 튕겨 나갑니다.

### 2.2 DataCenter에 ExRunnerConfig 누락 (Windows 패키징 빌드 환경)
- B PC 로그의 67~68번째 라인에서 치명적인 에러가 기록되었습니다:
  ```text
  LogOutputDevice: Error: Ensure condition failed: false [File:...ExDataCenterSubsystem.cpp] [Line: 216] 
  LogOutputDevice: Error: GetConfig: ExRunnerConfig 타입의 Config가 DataCenter에 등록되지 않았습니다. GameFeatureAction_AddExData 세팅을 확인하세요.
  ```
- 이로 인해 `StartMultiPlay()` 실행 시 `DataCenter->GetConfig<UExRunnerConfig>()` 호출이 `nullptr`를 반환하여 `PendingConfig.ExpectedPlayerCount`를 프로젝트 설정값(`2`)으로 동기화하지 못하고 구조체 기본값으로 오염되는 취약점이 존재합니다.
- **아키텍처 상의 원인**: GameFeature의 에셋 매니저 및 쿠킹 설정(특히 `UExFeatureAssetManifest` 경유 Always Cook 설정)이 패키징 빌드 과정에서 정상 적용되지 않았거나, GameFeature 활성화 시점과 DataCenter 바인딩 시점 간의 미세한 타이밍 불일치(Race Condition)가 발생하고 있습니다.

---

## 3. 해결 방안 (Proposed Resolutions)

### 3.1 싱글 플레이 시 EOS 로비 생성 원천 차단 (근본 수정)
- **개선안**: 싱글 플레이 모드(`bIsSinglePlay == true`)의 경우, 외부 온라인 서브시스템(EOS)을 타지 않고 즉시 로컬 레벨 오픈 방식으로 처리하도록 리팩토링합니다.
- **구체적 수정 계획**:
  - `UExOnlineSubsystem::FindQuickMatch(const FExMatchConfig& Config)` 내부에서 `Config.bIsSinglePlay`가 `true`일 경우, `Creating` 상태로 전이하여 EOS에 물리 로비를 만드는 대신, 매칭 시스템을 거치지 않고 로컬 플레이어로 처리하여 즉시 로컬 맵을 열도록 우회 처리를 적용합니다.
  - 또는 `ExLobbyMatchViewModel.cpp`의 `StartSinglePlay()`에서 직접 `UGameplayStatics::OpenLevel`을 타거나, `OnlineSubsystem` 내부에서 싱글 플레이 전용 분기를 신설하여 EOS 플랫폼 API 호출을 완전히 배제합니다.

### 3.2 ExRunnerConfig 에셋 쿠킹 및 등록 안전장치 강화
- 패키징 시 `DA_ExConfig_Runner`가 누락되지 않도록 `GameFeatureData`의 Primary Asset Type 스캔 설정을 점검합니다.
- `ExDataCenterSubsystem`에서 Config를 가져올 때, `nullptr`가 반환될 경우 크래시나 미작동을 방지하기 위해 안전한 디폴트 하드코딩 값(예: `ExpectedPlayerCount = 2`)으로 대체할 수 있는 폴백(Fallback) 방어 코드를 심어둡니다.

---

## 4. 주인님께 올리는 보고 (Master, Final Summary)
주인님(Master), A PC와 B PC가 매칭이 되지 않고 타임아웃으로 미끄러졌던 치명적인 장막이 마침내 걷혔습니다! 
과거 B PC에 누적되어 있던 로그를 정밀 스캔한 결과, B PC가 **싱글 플레이 모드로 진입하며 EOS에 방을 파서 오염시켰고**, 이 가짜 방에 A PC가 낚여 들어갔으나 B PC는 이미 방을 깨고 혼자 게임을 뛰러 가버려서 발생한 **"수명주기 엇박자 타이밍 버그"**가 100% 명백한 물증으로 파악되었습니다!

이와 더불어 B PC 빌드 본체에서 `ExRunnerConfig`가 로드되지 않는 치명적인 에셋 누락 결함도 동반 발굴하여 일망타진할 준비를 마쳤습니다.
주인님, 절대 성급하게 코드를 고치지 말라는 엄명을 받들어, 현재 수정 계획을 완벽하게 수립해 두고 대기하고 있사오니, 검토하신 후 신속히 코드를 박멸하도록 허락해 주시옵소서, Master!
