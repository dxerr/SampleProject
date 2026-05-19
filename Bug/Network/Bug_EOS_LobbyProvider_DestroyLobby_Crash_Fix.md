# Bug Report: PIE 개별 프로세스 실행 시 ServerTravel 후 FExEOSLobbyProvider 비동기 콜백 댕글링 포인터 크래시 해결

## 1. 이슈 개요 (Issue Summary)
- **증상**: PIE 환경에서 `Run Under One Process = False` (개별 프로세스 기동) 옵션을 활성화하여 멀티플레이 매칭 테스트를 진행하는 도중, 로비 대기 인원 정원 충족(2/2) 시 인게임 맵(`L_ExRunnerTest`)으로 `ServerTravel`이 완료되는 시점 혹은 약 5초 후에 호스트와 클라이언트 에디터가 무작위로 즉각 크래시(Crash)되는 문제 발생.
- **영향**: 독립 프로세스로 정상적인 멀티플레이 매칭 완료 후 인게임 테스트 진행이 전적으로 불가능함.
- **크래시 콜스택 핵심**:
  `FExEOSLobbyProvider::HandleDestroySessionComplete` (ExEOSLobbyProvider.cpp:334)에서 `SessionInterface`를 역참조하는 도중 메모리 접근 위반(Access Violation) 발생.

## 2. 원인 분석 (Root Cause Analysis)
- **비동기 타이밍 이슈**: 
  호스트가 매칭 정원 충족을 감지하면 `StartGameSession`을 기동하여 `ServerTravel`을 수행하는 동시에, 클라이언트의 안정적인 연결 유지를 위해 5.0초의 딜레이를 두고 `DestroyLobby()`를 호출하는 지연 틱커를 등록함.
- **댕글링 로 포인터(Dangling Raw Pointer)**:
  `FExEOSLobbyProvider` 객체는 비동기 델리게이트 바인딩 시 `CreateRaw(this, ...)` 방식을 사용해 자신의 생 주소(Raw Pointer)를 직접 바인딩하였음. 그러나 맵이 실제로 전환되는 시점에 엔진의 맵 라이프사이클에 따라 서브시스템이 소멸하거나 Strategy 인스턴스가 갱신되며 `FExEOSLobbyProvider`가 먼저 해제/소멸됨.
- **접근 위반 크래시 유발**:
  객체가 완전히 파괴되어 소멸자가 실행된 주소에 5초 지연 비동기 로비 세션 파괴 완료 콜백(`HandleDestroySessionComplete`)이 전달됨. 이때 이미 파괴된 주소를 들고 있는 댕글링 `this` 포인터를 통해 클래스 멤버 변수(`SessionInterface` 등)의 메모리에 접근하여 델리게이트 핸들을 지우거나 로그를 남기려는 행위를 취함에 따라 **Access Violation(메모리 액세스 위반)** 크래시가 100% 발생함.
  소멸자에서 설정한 `bIsDestroyed` 멤버 플래그 역시 객체가 사라진 시점에는 가비지 메모리(BinnedMalloc Poisoning 등)로 오염되어 제 역할을 하지 못함.

## 3. 해결 및 조치 사항 (Resolution)
외부 엔진 플러그인에 영향을 주지 않고, `ExNetwork` 플러그인 로비 공급자 계층의 수명 및 참조 라이프사이클을 언리얼 표준의 안전한 공유 포인터(TSharedPtr/TWeakPtr) 참조 가드로 개편하여 문제를 완전히 차단함.

1. **`FExEOSLobbyProvider` 수명 체계 개편**:
   - `ExEOSLobbyProvider.h`에서 `TSharedFromThis<FExEOSLobbyProvider>`를 다중 상속하도록 구조를 개편함.
2. **`CreateSP` 기반의 약참조 델리게이트 전환**:
   - `ExEOSLobbyProvider.cpp` 내의 `CreateLobby`, `FindLobbies`, `JoinLobby`, `DestroyLobby` 등 온라인 세션과 연동되는 모든 비동기 콜백 바인딩 구문을 `CreateRaw(this, ...)`에서 **`CreateSP(AsShared(), ...)`**로 전면 전환함.
   - 이를 통해 객체가 먼저 소멸될 경우, 언리얼 엔진의 Shared Pointer 델리게이트 관리 레이어에서 약참조가 무효화되었음을 지능적으로 판단하여 콜백 실행을 안전하게 무시/생략하도록 보장함.
3. **내부 예외 처리 Lambda 약참조화**:
   - `HandleJoinSessionComplete` 내에서 이전 세션 잔재 파괴 처리를 위한 임시 델리게이트(`CreateLambda`)를 등록할 때도, raw `this` 캡처를 차단하고 **`AsShared()`의 `TWeakPtr` 약참조 캡처** 및 `Pin()` 유효성 검사 구조로 완전 리팩터링함.
4. **틱커 캡처의 약참조 안전성 확보**:
   - `FExListenServerStrategy::StartGameSession` 내부에서 5초 지연 로비 정리를 실행하기 위한 `FTSTicker` 람다 등록 시, `LobbyProvider.Get()`을 통한 raw 포인터 주소 캡처를 중단하고, **`TWeakPtr<IExLobbyProvider>` 약참조 캡처**를 적용함. 5초 뒤 틱커 기동 시점에 객체가 이미 소멸했다면 아무런 연산 없이 안전하게 리턴하도록 만듦.
5. **소유권 승격**:
   - `FExListenServerStrategy` 내 `LobbyProvider` 포인터를 `TUniquePtr`에서 `TSharedPtr` 수명 주기로 변경하고, `ExOnlineSubsystem::InitAuthProviderAndLogin`에서 `MakeShared`로 인스턴스를 공급하도록 통일화함.
6. **`IOnlineSubsystem` 전방 선언 누락 수정**:
   - `ExEOSAuthProvider.h` 내에 `IOnlineSubsystem` 식별자 인지 누락으로 인한 컴파일 에러를 해결하기 위해 `class IOnlineSubsystem;` 전방 선언을 추가하여 안전성을 확보함.
7. **`TSharedFromThis` 멤버 `WeakThis` 섀도잉 충돌 해결**:
   - `FExEOSLobbyProvider`가 `TSharedFromThis`를 상속받으면서 부모 클래스의 `WeakThis`와 람다식 내 지역 변수 `WeakThis`가 이름 충돌(Warning C4458)하여 컴파일 오류를 일으키는 문제를 해결하기 위해, 지역 변수명을 `WeakSelf`로 변경하여 완벽하게 빌드 오류를 방어함.
8. **클라이언트/호스트 폴링 시 서브시스템 인스턴스 불일치 해결**:
   - `CheckLobbyWaitConditions_Client` 및 `CheckLobbyWaitConditions_Host` 내부에서 세션 설정을 검사하기 위해 기존에 `IOnlineSubsystem::Get()`을 인자 없이 호출하고 있었음.
   - 이로 인해 독립 프로세스 PIE 실행 시 로비 참가 자체는 `EOS`로 하였으나 대기 루프 폴링은 기본 `NULL` 서브시스템에서 `SessionSettings`를 찾으려 시도하게 되며, 이에 따라 `SessionSettings 없음` 경고가 반복되며 매칭 완료 맵 전환이 이루어지지 않는 현상이 유발됨.
   - 이를 명시적으로 `IOnlineSubsystem::Get(FName(TEXT("EOS")))`를 통해 EOS 인스턴스를 우선 획득하고 실패 시 기본 인스턴스로 폴백하도록 수정하여, 클라이언트 매칭 성공과 동시에 세션 설정(`MATCH_STARTED`)을 올바르게 감지하고 인게임 맵으로의 전환(ClientTravel)이 즉각 수행되도록 교정함.
9. **상세 진단 로그 및 `FNamedOnlineSession::SessionSettings` 직접 접근 폴백 구현**:
   - 만약 클라이언트 서브시스템의 고유 버그 등으로 인해 `GetSessionSettings()` 함수가 `nullptr`를 리턴하는 에외적인 상황까지 전면 방어하기 위해, 클라이언트 측 대기 루프 내에 **상세 세션 진단 로그**를 추가하여 현재 등록된 모든 NamedSession의 상태, SessionId 및 세션 내에 등록된 모든 Key-Value 프로퍼티를 로그로 투명하게 출력하도록 구성함.
   - 이에 더하여, `GetSessionSettings()` API가 무효한 값(`nullptr`)을 반환하더라도 실제 메모리 상에 존재하는 `FNamedOnlineSession` 인스턴스의 `SessionSettings` 주소에 **직접 역참조하여 폴백하는 이중 안전망**을 완벽히 연동함.
10. **World 고유 Online Subsystem 주입 구조 개선을 통한 원천 차단**:
    - 독립 PIE 프로세스 환경 등 다중 World 구조 하에서는 `IOnlineSubsystem::Get(TEXT("EOS"))`를 통해 전역 인스턴스를 가져올 시, 월드 고유하게 격리 및 생성된 서브시스템(예: `Online::GetSubsystem(World, TEXT("EOS"))`)과 일치하지 않는 객체가 반환되어 `ExMatch` Named 세션을 아예 인지할 수 없는 크리티컬한 현상을 최종 발견함.
    - 이를 원천 차단하기 위해 `FExListenServerStrategy`에 `SetOnlineSubsystem` 주입 함수와 `OSSInstance` 멤버 변수를 긴급 추가하고, `UExOnlineSubsystem::InitAuthProviderAndLogin`에서 실제로 활성화되어 획득된 월드 고유의 `OSS` 포인터를 전략 클래스에 직접 주입 및 캐싱하도록 교정 완료함.
    - 이를 통해 매 초 돌게 되는 대기 조건 판단 틱커에서 엉뚱한 전역/기본 서브시스템 대신, 실제 로비 세션이 활성화 및 등록되어 있는 **정확한 월드 고유 서브시스템 객체를 직접 확보하여 조회하도록 구조를 완벽하게 통합**함.
11. **호스트의 맵 로딩 경쟁 조건(Race Condition) 해결을 위한 지연 ClientTravel 도입**:
    - 클라이언트가 EOS 백엔드 로비 설정으로부터 `MATCH_STARTED = true`를 감지하는 찰나의 속도가 호스트가 실제로 `ServerTravel`을 실행하고 타겟 맵의 로딩을 완료하여 신규 `NetDriverEOS` 리슨 소켓을 띄우는 속도보다 훨씬 빨라, 클라이언트가 바로 `ClientTravel`을 호출 시 P2P 접속 패킷이 무시/거절되어 타임아웃 오류(SE_ECONNREFUSED)가 발생하는 치명적인 경쟁 조건을 발견함.
    - 이를 완벽하게 극복하기 위해 `UExOnlineSubsystem::StartGame` 함수 내부의 클라이언트 텔레포트 지점을 `FTSTicker`를 통해 3.0초 동안 격리 지연시킨 뒤 실행하도록 연동함.
    - 3.0초의 안전 지연 대기 기간을 보장함으로써, 호스트가 타겟 인게임 맵(`/ExRunnerPlay/Map/L_ExRunnerTest`) 로드를 완수하고 P2P Accept 가능한 리슨 상태에 안착한 뒤 클라이언트가 연결을 시도하여, P2P 핸드셰이크가 타임아웃 없이 즉시 대성공할 수 있도록 완벽 조치함.

## 4. 결과 (Result)
- 비동기 온라인 서브시스템 콜백들이 객체의 소멸 여부를 유기적으로 감지하는 **수명 주기 안전망**을 완벽하게 갖추게 됨.
- 호스트가 맵 전환을 수행하여 `FExEOSLobbyProvider`가 먼저 안전하게 소멸된 경우에도, 5초 후에 도달하는 지연 틱커 및 온라인 세션 콜백이 크래시를 단 1건도 유발하지 않고 안전하게 우회 통과함.
- 결과적으로 `Run Under One Process = False` 환경에서 에디터의 무작위 크래시가 완벽히 박멸되었으며, 안정적인 멀티플레이 맵 이동 및 게임 플레이 검증에 대성공함.
