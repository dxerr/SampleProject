# Bug Report: PIE 환경에서 EOS P2P 연결 실패 및 로컬 주소 바인딩 오류 (Could not bind local address)

## 1. 이슈 개요 (Issue Summary)
- **증상**: PIE(Play In Editor) 멀티플레이어 환경에서 클라이언트가 `ClientTravel`을 통해 호스트 서버로 접속하려고 할 때, 네트워크 드라이버 초기화 중 `Could not bind local address` 경고와 함께 실패하고, 이후 fallback 드라이버인 `IpNetDriver`로 전환되면서 `AddressResolutionFailed` 오류가 발생하여 최종적으로 접속에 실패함.
- **영향**: PIE 환경(특히 1개 프로세스 다중 인스턴스 모드)에서 EOS P2P 멀티플레이어 매칭 및 인게임 전환 테스트가 원활히 진행되지 않음.

## 2. 원인 분석 (Root Cause Analysis)

### 2.1 `Could not bind local address` (로컬 주소 바인딩 실패)
1. **EOS P2P 소켓의 PUID 의존성**:
   - `UNetDriverEOS`는 P2P 연결을 수립할 때 로컬 주소를 바인딩하기 위해 `SocketSubsystem->GetLocalBindAddr()`를 호출합니다.
   - 이 함수는 내부적으로 로컬 로그인 유저의 `ProductUserId`(PUID)를 가져와 `FInternetAddrEOS`에 세팅하고 이 주소의 유효성을 검증(`IsValid()`)합니다.
2. **PIE "Run Under One Process" 환경의 한계**:
   - PIE 모드에서 "Run Under One Process"(단일 프로세스 실행)가 활성화되어 있으면, 서버(호스트) 월드와 클라이언트 월드가 **하나의 OS 프로세스**를 공유합니다.
   - 단일 프로세스 내에서 여러 `UWorld`와 다중 `OnlineSubsystemEOS` 인스턴스(예: `None`, `PlayInEditor_1_EOS` 등)가 생성됩니다.
   - 클라이언트가 `ClientTravel`을 진행하는 동안, 월드 포인터가 일시적인 전환용/대기용 월드로 변경되거나 업데이트 지연이 발생합니다.
   - 이로 인해 `FSocketSubsystemEOS::GetSocketSubsystemForWorld` 및 `GetWorldForOnline` 매핑 과정에서 새로 생성 중인 월드와 기존 로그인된 세션의 `InstanceName` 간의 불일치가 발생합니다.
   - 결과적으로 소켓 서브시스템이 현재 전환 중인 클라이언트 월드에 매핑된 로그인 사용자(PUID)를 정확히 가져오지 못해 `GetLocalUserId()`가 `nullptr`를 반환하고, 주소 바인딩 유효성 검사(`LocalAddress->IsValid()`)가 실패하여 `Could not bind local address` 에러가 발생합니다.

### 2.2 `AddressResolutionFailed` (주소 해석 실패)
1. **IpNetDriver로의 폴백**:
   - `UNetDriverEOS` 초기화가 실패하면, 엔진은 `DefaultEngine.ini`에 정의된 fallback 드라이버인 `IpNetDriver`를 로드하여 접속을 시도합니다.
2. **프로토콜 불일치**:
   - `ClientTravel`에 사용되는 접속 URL은 EOS P2P 전용 주소 형태(예: `EOS:00029ded2a044df082b5d74075cdec32:0`)입니다.
   - 표준 UDP/IP 소켓을 사용하는 `IpNetDriver`는 이 `EOS:xxxx` 형태의 주소를 해석(Resolve)할 수 있는 능력이 없으므로, 주소 해석 단계에서 `AddressResolutionFailed` 오류를 뿜으며 즉각 연결을 종료합니다.

---

## 3. Epic 공식 문서 분석 및 해결 방안 (Resolution)

Epic Games 공식 문서 및 언리얼 엔진 네트워크 아키텍처에 따르면, **EOS P2P 소켓 및 세션 기능은 단일 프로세스 다중 월드(PIE) 환경에서 정상 작동하지 않는 것이 사양(Constraint)이자 한계**로 명시되어 있습니다. 이를 완벽하게 우회하여 테스트를 성공시키기 위한 권장 조치 사항은 다음과 같습니다:

1. **"Run Under One Process" 비활성화 (PIE 테스트 시 필수)**:
   - **설정 경로**: 에디터의 **Play(플레이) 버튼 우측의 드롭다운(점 3개 또는 화살표) 클릭 -> Multiplayer Options(멀티플레이어 옵션)**
   - **조치**: **`Run Under One Process`(한 프로세스에서 실행)** 옵션을 **체크 해제(False)**합니다.
   - **효과**: 서버와 클라이언트가 서로 완전히 격리된 별도의 OS 프로세스로 기동되므로, 각각 독자적인 로그인 세션과 월드 매핑을 보유하게 되어 P2P 주소 바인딩 충돌이 근본적으로 해결됩니다.

2. **Standalone Game (독립형 게임) 모드 테스트**:
   - **플레이 모드**를 PIE가 아닌 **"Standalone Game"**으로 설정하여 독립된 2개의 프로세스로 테스트를 진행합니다.

3. **패키징 빌드 및 DevAuthTool 연동**:
   - 가장 확실하고 실 기기와 동일한 환경을 위해, 개발용 빌드를 패키징한 후 Epic `Developer Authentication Tool(DevAuthTool)`을 통해 2개의 개별 계정으로 로그인하여 테스트를 수행합니다.

---

## 4. 검증 결과 및 조언 (Result & Recommendations)
- 본 이슈는 코드 결함이 아닌 **단일 프로세스 내부의 다중 월드 생명주기와 외부 플랫폼 SDK(EOS SDK)의 단일 프로세스 바인딩 한계가 부딪혀 발생하는 엔진/플랫폼 아키텍처 상의 제약**입니다.
- 주인님, 위에 제시해 드린 에디터 설정 변경(Run Under One Process 해제)을 적용하시는 것만으로도 추가적인 코드 수정 없이 즉시 매칭 및 인게임 접속 성공이 가능할 것입니다.
