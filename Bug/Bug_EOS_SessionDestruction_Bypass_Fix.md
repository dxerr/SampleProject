# Bug Report: 멀티플레이 매칭 시 로비 세션 파괴 및 로그인 소실 버그 해결 (방법 B)

## 1. 이슈 개요 (Issue Summary)
- **증상**: 2인 멀티플레이어 환경에서 대기 로비의 정원이 모두 차는 시점(2/2)에 맵 전환(ServerTravel)이 발생한 직후, 로컬 플레이어가 갑자기 로그아웃(Log Out)되고 생성되어 있던 로비 세션이 완전히 공중 분해되는 치명적인 멀티플레이 매칭 버그 발생.
- **영향**: 정원이 가득 찬 직후 인게임으로 넘어가지 못하고 세션 연결이 끊어지며 강제로 메인 화면으로 이탈함.

## 2. 원인 분석 (Root Cause Analysis)
- **중복 자동 로그인 기동**: 맵 전환이 발생하면서 새로운 맵의 `AGameModeBase::InitGame` 수명주기 내에서 `GameSession->ProcessAutoLogin()`을 자동으로 실행함.
- **플랫폼 인증 토큰 부재**: PC 로컬 개발 환경(Steam/Epic App 등이 실행되지 않은 상태)에서는 플랫폼 API를 통한 외부 인증 토큰(`GetPlatformAuthToken`)을 획득할 수 없음.
- **파괴적인 예외 처리**: `FUserManagerEOS::AutoLogin` 과정 중 플랫폼 인증에 실패하면서 `ConnectLoginNoEAS`가 오류 핸들러를 타게 되고, 이 과정에서 기존에 이미 기동 중이던 `LocalUsers[0]`의 유효한 `UniqueNetId` 정보를 통째로 날려버리는 `RemoveLocalUser(0)`를 수행함.
- **로비 파괴**: 기기 내 로그인 정보가 완전히 소실(NotLoggedIn 상태로 폴백)되자, 온라인 서브시스템의 로비 매니저가 유저의 강제 이탈을 감지하여 동작 중이던 P2P 로비 세션을 강제로 파괴(Lobby Destroyed)해 버림.

## 3. 해결 및 조치 사항 (Resolution)
외부 서드파티 플러그인(`OnlineSubsystemEOS`)의 내부 코드를 강제로 건드리지 않고, 프로젝트의 `ExCore` 모듈 수준에서 깔끔하고 안전하게 버그를 방어하는 **방법 B(커스텀 GameSession 구현)**를 채택하여 해결함.

1. **커스텀 GameSession 클래스 추가**:
   - `Plugins/GameFeatures/ExCore/Source/ExCoreRuntime/GameModes/` 디렉터리에 `AExGameSession` 클래스를 새롭게 추가함.
2. **`ProcessAutoLogin` 재정의**:
   - `ProcessAutoLogin()`을 오버라이드하여, 이미 유효하게 동작하고 있는 로그인 세션이 있을 경우 불필요하고 파괴적인 중복 자동 로그인을 차단함.
   - 즉시 `true`를 반환하도록 하여 언리얼 엔진의 기본적인 게임 모드 초기화 흐름을 방해 없이 원활하게 보장함.
3. **GameModeBase 생성자 반영**:
   - `AExGameModeBase::AExGameModeBase()` 생성자 내에서 `GameSessionClass = AExGameSession::StaticClass();`를 할당하여 엔진이 기존 기본 GameSession 대신 당사 커스텀 클래스를 로드하도록 조치함.

## 4. 결과 (Result)
- 맵 전환 시 호스트 서버 로그에 다음 디버그 로그가 정상 출력됨:
  `[AExGameSession] 주인님, 중복 자동 로그인 요청(ProcessAutoLogin)을 안전하게 차단하여 기존 EOS 세션을 전적으로 보존합니다.`
- 더 이상 자동 로그인 오류에 의해 기존 로그인 세션이 소실되지 않으며, 로비가 폭파되는 일 없이 호스트와 참가 클라이언트 모두 안전하게 인게임 맵(`L_ExRunnerTest`)에 정상 진입 및 안정적으로 연결을 유지하는 데 성공함.
