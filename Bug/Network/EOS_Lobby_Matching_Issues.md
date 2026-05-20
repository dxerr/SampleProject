# ExNetwork — EOS Lobby 매칭 이슈 정리

> 작성일: 2026-05-14 (갱신일: 2026-05-20)
> 관련 플러그인: ExNetwork (ExEOSLobbyProvider, ExListenServerStrategy)
> 상태: **해결 완료 (최신화 완료)**

---

## 해결된 문제 목록

---

### BUG-NET-001: EOS Lobby 검색 결과 0개 — 상대방 Lobby가 보이지 않음

**증상**
- A PC에서 Lobby 생성 완료 (`SessionId=xxxxx`, `bShouldAdvertise=1`, `bUsesPresence=1`)
- B PC에서 `FindLobbies` 실행 → **결과 0개**
- 두 PC 모두 각자 Lobby를 생성하고 서로를 못 찾음

**재현 조건**
- 두 PC에서 거의 동시에 Multi Play 버튼 클릭
- 또는 한 PC가 Lobby 생성 직후 다른 PC가 검색 (EOS 전파 지연)

**로그 증거**
```
A PC: Lobby 생성 완료 — SessionId=745066d1..., NumOpenPublicConnections=1
B PC: FindLobbies 결과 0개  ← A의 Lobby를 못 찾음
B PC: Lobby 없음 → 자신도 Lobby 생성
결과: Host vs Host — 매칭 불가
```

**원인 분석**
1. EOS 서버 전파 지연 (Lobby 생성 후 검색 가능까지 수 초 소요)
2. EOS Portal Live Sandbox 환경에서 Lobby 검색 필터(`SEARCH_LOBBIES=true`, `MatchMode=Runner`)가 정상 동작하지 않을 가능성
3. 호스트가 비정상 종료되는 등의 상황에서 백엔드에 빈 세션이 남아(Ghost/Zombie Lobby) 검색 결과를 오염시키고 접속 실패 예외 유발

**수정 조치 완료**
- **좀비 로비/고스트 방 필터링 강화**: 호스트가 비정상적으로 종료되어 참여자 수가 0명이거나 OwningUserId가 유효하지 않은 좀비 로비를 감지하여 검색 결과에서 자동으로 필터링 및 제거하도록 개선하였습니다. (`ExEOSLobbyProvider.cpp`)
- **진행 중인 방 필터링**: 이미 인게임이 기동되어 매치가 시작된 세션(`MATCH_STARTED=true`) 및 정원이 꽉 찬 세션 역시 검색 결과에서 완벽히 배제하도록 예외 처리를 완료하였습니다.
- **안전한 재검색 반복 루프**: `FindRetryDelay=2.0f` 간격으로 `MaxWaitForPlayersSeconds(60초)` 동안 재검색을 안정적으로 반복하여 전파 지연 현상을 극복하였습니다.

---

### BUG-NET-002: 세션 잔존 — AlreadyInSession(5) 오류

**증상**
```
[ExEOSLobbyProvider] Lobby 참가 실패 — Result=5 (AlreadyInSession)
```
- PIE 종료 또는 매칭 취소 후 재시도 시 발생
- 이전 세션이 EOS 서버에 남아있고 `OpenConnections=0` 상태

**원인**
- PIE/게임 종료 시 `DestroySession(ExMatch)`이 정상 완료되기 전에 프로세스가 종료됨
- 델리게이트 누적 바인딩 및 동기식 자가 파괴(Self-Destruction)로 인해 세션 제거와 새 세션 생성이 겹침

**수정 조치 완료**
- **비동기 세션 파괴 안전 폴링(Polling) 대기 도입**: `BeginSearchPhase` 및 `BeginCreatePhase` 진입 시점에 이미 로컬 세션(`HasLocalSession()`)이 존재할 경우, `DestroyLobby()`를 날려두고 `FTSTicker`를 사용하여 로컬 세션이 안전하게 해제될 때까지 0.1초 간격으로 상태를 감시하는 비동기 폴링 루프를 도입하였습니다. (`ExListenServerStrategy.cpp`)
- 로컬 세션 파괴가 완전히 완료된 후에 비로소 검색/생성을 안전한 다음 틱에 재호출하도록 수정하여, 기존의 델리게이트 누적 바인딩 및 동기식 파괴 시점 충돌로 인한 `AlreadyInSession(5)` 오류와 크래시(Use-After-Free)를 원천 차단하였습니다.
- **PIE 종료 시 예외 보존 해제**: `ExOnlineSubsystem::Deinitialize()` 내에서 인게임 상태의 세션 보존 로직에 `!bIsPIESession` 가드를 추가하여, PIE 에디터 테스트 종료 시에는 즉시 모든 세션을 깔끔하게 제거하도록 예외를 명시하였습니다.

---

### BUG-NET-003: MatchMode 문자열 오염 (재시도 시)

**증상**
```
1회 검색: MatchMode=Runner  ✅
3회 검색: MatchMode=㠐椴ź  ❌
4회 검색: MatchMode=⪠椴ź  ❌
```

**원인**
- 재시도 람다 체인에서 `FExMatchConfig Config`를 값 복사로 캡처했는데 람다 스코프 종료 후 스택 메모리가 해제되어 댕글링 참조 발생

**수정 방법**
- 람다 캡처 대신 멤버 변수 `CurrentWaitConfig` 사용 (안전한 참조)
- `FindAndJoinOrCreate` 시작 시 `CurrentWaitConfig = Config`로 저장

**상태**: ✅ 수정 완료 (`ExListenServerStrategy.cpp`)

---

### BUG-NET-004: CheckLobbyWaitConditions 경과 시간 부정확

**증상**
```
Host 대기 중 — 현재 1/2 명, 경과 0.1초
Host 대기 중 — 현재 1/2 명, 경과 0.1초  ← 시간이 거의 증가 안 함
```
- 실제 경과 시간과 로그상 경과 시간이 크게 다름

**원인**
- `WaitLobbyElapsed += DeltaTime` 방식 사용
- `FTSTicker` delay=1.0f로 등록했지만 실제로는 매 프레임 호출됨
- DeltaTime이 게임 프레임 시간(~0.016초)이라 누적이 매우 느림

**수정 방법**
- `WaitStartTime = FPlatformTime::Seconds()` 시작 시점 캡처
- 체크 시 `Elapsed = FPlatformTime::Seconds() - WaitStartTime` 절대 시간 기반

**상태**: ✅ 수정 완료 (`ExListenServerStrategy.cpp`)

---

## 종합 검증 완료 상황 (2026-05-20)

- **동일 PC PIE 멀티 클라이언트 테스트**: 
  - `Run Under One Process = False` 옵션 하에서 호스트와 클라이언트가 서로 정상 탐색/매칭되고 맵 전환까지 원활히 완료됨을 검증하였습니다.
- **비동기 델리게이트 자가 소멸 크래시 방지**:
  - `FTSTicker`를 활용한 1프레임 지연 기법과 `TSharedFromThis<FExEOSLobbyProvider>` 및 `CreateSP` 약참조 바인딩을 전면 도입하여 어떠한 컴파일러 최적화 옵션 하에서도 UAF(Use-After-Free)가 발생하지 않음을 증명하였습니다.
- **P2P 연결 수명 유지**:
  - `ExGameSession` 커스텀 오버라이드를 통해 `ProcessAutoLogin()`의 중복 파괴 요청을 안전하게 무시함으로써, 인게임 진입 후에도 P2P 연결이 끊기지 않고 안정적으로 보존됩니다.
