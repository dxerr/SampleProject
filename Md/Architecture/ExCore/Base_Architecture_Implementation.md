# 멀티플레이어 아키텍처 베이스 클래스 구현 동기화 리포트

> **작성일:** 2026-03-04
> **대상 모듈:** ExCore

본 문서는 `ExFrameWork_Multiplayer_Flow_Architecture.md` 스펙을 바탕으로, 
엔진 내에 C++로 1차 구현된 베이스 클래스들의 변경점 및 활용 규칙을 팀에 동기화하기 위한 리포트입니다.

## 1. 전역 아키텍처 및 상태 전이 제어
언리얼 내장 `UGameInstanceSubsystem`과 글로벌 태그 리지스트리를 결합하여 
게임 라이프사이클의 4단계(부팅 -> 인증 -> 로비 -> 인게임)를 강력히 캡슐화했습니다.

* **관련 클래스:** 
  * `ExFlowTags.h` / `.cpp` (Native Gameplay Tags)
  * `UExGameFlowSubsystem`
* **주요 특징:**
  * **설계 한계 극복:** `GameInstance` 서브시스템은 네트워크 복제가 안 된다는 한계가 있으므로, 클라이언트의 상태 전이는 UI의 브로드캐스팅(`SetFlowState` 호출) 또는 게임모드/게임스테이트를 우회한 Client RPC를 통해 수동으로 동기화되어야 함을 코드로 명확히 했습니다.
  * **Travel 제어의 분리:** 서브시스템은 맵 이동 권한이 없습니다. 대신 `OnRequestTravel` 델리게이트를 통해 이벤트를 쏘면, 이를 구독하고 있는 서버 권한의 `AExGameModeBase`가 실제 `ServerTravel`을 타도록 "관심사의 분리(Separation of Concerns)"를 달성했습니다.

## 2. 매치 상태 동기화 및 난입(JIP) 대응
데디케이티드 서버를 중심으로 매치 상황(대기 -> 카운트다운 -> 플레이 -> 결과)을 단일 서버가 제어하고 모든 클라이언트에게 신뢰성 있게 복제되도록 구축했습니다.

* **관련 클래스:**
  * `ExMatchTags.h` / `.cpp` (Native Gameplay Tags)
  * `AExGameModeBase`
  * `AExGameStateBase`
* **주요 특징:**
  * **단방향 제어:** `AExGameModeBase::SetMatchPhase`에서만 전이를 지시하며, 상태값인 `CurrentMatchPhase`는 `AExGameStateBase`에 귀속되어 모든 클라이언트로 전파(Replicate)됩니다.
  * **난입 가드(JIP Guard):** 나중에 들어오는 유저는 델리게이트를 못 받아 UI가 안 뜨는 이슈를 방지하기 위해, `AExGameStateBase::BeginPlay()` 시점에 로컬 클라이언트라면 한번 강제로 이벤트를 쏘아주는(Broadcast) 초기화 가이드를 이식했습니다.

## 3. 플레이어 상태 및 넷 대역폭 효율화
불필요한 인티저 점수 변수 할당을 방지하고 언리얼의 로우레벨 리플리케이션 효율을 끌어올렸습니다.

* **관련 클래스:**
  * `AExPlayerStateBase`
* **주요 특징:**
  * 엔진 내장 `Score`(float) 변수와 `OnRep_Score()` 가상함수를 래핑하여 다시 구현함으로써, 컴포넌트 변수 중복을 없앴습니다. UI는 `OnScoreChangedDelegate`만 구독하면 자동으로 점수 변화를 감지합니다.

## 4. 인벤토리 & 백엔드 (향후 확장 기반)
아직 상세 기능은 없지만, 클라이언트가 서버로 액션을 요청하는 Server RPC 구조의 정석을 뼈대로 잡았습니다.

* **관련 클래스:**
  * `UExInventoryComponent`
  * `UExBackendCommunicationSubsystem`
* **주요 기능:**
  * 어떠한 컨트롤러/폰에 부착되어도 정상 작동하는 `HasAuthority()` 위주의 판별 로직 적용.
  * HTTP 요청 및 로비 웹 인증의 상태 머신(Idle -> Requesting -> Success/Fail).
