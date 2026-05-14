# ExNetwork — EOS Lobby 매칭 이슈 정리

> 작성일: 2026-05-14
> 관련 플러그인: ExNetwork (ExEOSLobbyProvider, ExListenServerStrategy)
> 상태: **디버깅 진행 중**

---

## 현재까지 확인된 문제 목록

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
2. EOS Portal Live Sandbox 환경에서 Lobby 검색 필터(`SEARCH_LOBBIES=true`, `MatchMode=Runner`)가
   정상 동작하지 않을 가능성
3. EOS Client Policy에서 Lobby 검색 권한 미설정 가능성

**현재 적용된 대응**
- `FindRetryDelay=2.0f` 간격으로 `MaxWaitForPlayersSeconds(60초)` 동안 재검색 반복
- 타임아웃 도달 후에만 Lobby 생성 (즉시 생성 제거)

**미해결 사항**
- A PC가 Lobby를 생성했는데 B PC의 어떤 검색에서도 0개가 반환됨
- EOS Portal에서 Live Sandbox → Live Deployment의 클라이언트 정책 설정 검토 필요
- `SEARCH_LOBBIES` 필터 없이 검색 시 결과가 나오는지 테스트 필요

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
- EOS 서버의 세션 TTL(Time-to-Live) 만료 전까지 잔존

**현재 적용된 대응**
- `HandleJoinSessionComplete`에서 `AlreadyInSession` 감지 시
  기존 로컬 세션 파괴(`DestroySession`) 후 자동 재참가

**미해결 사항**
- 세션 파괴 후 재참가 성공 여부 추가 테스트 필요

---

### BUG-NET-003: MatchMode 문자열 오염 (재시도 시)

**증상**
```
1회 검색: MatchMode=Runner  ✅
3회 검색: MatchMode=㠐椴ź  ❌
4회 검색: MatchMode=⪠椴ź  ❌
```

**원인**
- 재시도 람다 체인에서 `FExMatchConfig Config`를 값 복사로 캡처했는데
  람다 스코프 종료 후 스택 메모리가 해제되어 댕글링 참조 발생

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

## EOS 설정 현황 (2026-05-14 기준)

| 항목 | 값 |
|---|---|
| Product | Exit |
| ProductId | a63471008c394224b23772bf7fdb9b52 |
| SandboxId (Live) | 0f856c88b53b4d90be86d062c2fe2817 |
| DeploymentId (Live) | fade1999fa4643ce8add1351efe4c1f5 |
| ClientId | xyza7891ZSpoW4BCaFPZXB81AFEuqVJ6 |
| 클라이언트 정책 | Peer2Peer 타입 |
| bUseEAS | false |
| bUseEOSConnect | true |
| 로그인 방식 | Device ID (IOnlineIdentity::Login + externalauth:DeviceIdAccessToken) |
| 엔진 수정 | UserManagerEOS.cpp — GetPlatformDisplayName() 빈 문자열 시 "Player" fallback |

---

## 다음 확인 사항 (우선순위 순)

1. **EOS Portal Lobby 검색 권한 확인**
   - Live Deployment의 클라이언트 정책에 Lobbies `findLobbies` 권한 활성화 여부
   - `User required` 조건과 `Connect` 상호 배제 문제 재검토

2. **SEARCH_LOBBIES 필터 제거 후 테스트**
   - `SearchResults->QuerySettings.Set(SEARCH_LOBBIES, true, ...)` 라인 제거 후 재시도
   - EOS Lobby와 Session이 혼용될 때 이 필터가 오히려 검색을 막는지 확인

3. **MatchMode 필터 제거 후 테스트**
   - 모든 Lobby가 검색되는지 확인 (필터가 문제인지 분리)

4. **EOS SDK 버전 확인**
   - `SEARCH_LOBBIES` 상수가 현재 엔진 EOS SDK 버전에서 정의되어 있는지 확인

5. **동일 PC에서 PIE 멀티 클라이언트 테스트**
   - 같은 PC에서 에디터 3-Client PIE로 Lobby 생성/검색이 정상 동작하는지 먼저 확인
