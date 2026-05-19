# Bug Report: ExRunnerPositionSyncComponent 컴파일 에러

## 키워드
- Compile Error
- C2039
- C2737
- ServerAuthLocation
- Replicated

## 현상 (Issue)
`ExRunnerPositionSyncComponent.cpp` 컴파일 도중 `AExRunnerPlayerState`에 `ServerAuthLocation` 멤버 변수가 없다는 에러(C2039) 발생. 이전 로그의 잔재를 바탕으로 코드를 작성하였으나 실제 파일에는 해당 변수가 존재하지 않아 빌드 실패.

## 원인 분석 (Root Cause)
서버의 위치를 브로드캐스팅 하기 위해 `AExRunnerPlayerState`를 참조하도록 코드를 작성했으나, 해당 클래스에는 위치가 아닌 `ServerAuthPathDistance`(주행 거리) 변수만 존재했음. 

## 해결 과정 (Resolution)
1. PlayerState를 통해 서버 위치를 참조하는 잘못된 의존성 제거.
2. `UExRunnerPositionSyncComponent` 내부 자체에 `ServerAuthLocation` 변수를 선언.
3. 해당 변수에 `UPROPERTY(Replicated)` 속성을 부여하고, `GetLifetimeReplicatedProps` 함수를 통해 클라이언트로 직접 동기화(Broadcast) 하도록 구조를 개선.
4. `TickComponent`에서 서버인 경우(`HasAuthority()`) 자신의 위치를 변수에 업데이트하고, 클라이언트와 서버 양쪽 모두 해당 값을 기준으로 캡슐을 그리도록 로직 수정 완료.
