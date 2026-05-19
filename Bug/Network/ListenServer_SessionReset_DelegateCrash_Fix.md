# 버그 분석 및 해결 보고서: 비동기 세션 리셋 델리게이트 UAF 크래시

## 1. 이슈 개요
- **장소**: `FExListenServerStrategy::BeginSearchPhase` 및 `BeginCreatePhase`
- **현상**: 매칭 전 잔류 세션을 정리하기 위해 `DestroyLobby`를 호출하고 `OnDestroyComplete` 델리게이트를 통해 완료 처리를 받던 중, 델리게이트 내부 `Clear`로 인해 델리게이트 브로드캐스트 스택이 오염되거나(UAF - Use After Free) 크래시가 발생하는 현상.
- **키워드**: `ListenServer`, `SessionReset`, `OnDestroyComplete`, `Delegate Invalidation`, `Crash`, `Use-After-Free`

---

## 2. 원인 분석
- `OnDestroyComplete`는 멀티캐스트 델리게이트(`DECLARE_MULTICAST_DELEGATE_OneParam`)입니다.
- 매칭 재시도 및 세션 파괴가 일어날 때마다 `LobbyProvider->OnDestroyComplete.AddLambda(...)`가 **누적되어 바인딩**되었습니다.
- 비동기로 파괴가 성공적으로 완료되어 `Broadcast`되는 도중, 람다 내부에서 `SharedProvider->OnDestroyComplete.Clear()`를 동기적으로 호출하여 델리게이트 배열 자체를 청소해버렸습니다.
- 이로 인해 언리얼 엔진 델리게이트 리스트를 마저 순회(Loop)하려던 브로드캐스트 스택 내부(`DelegateInstancesImpl.h`, `MulticastDelegateBase.h`)에서 메모리 무효화(Iterator Invalidation)가 일어나 비정상 크래시가 발생하게 되었습니다.

---

## 3. 해결 방안 (폴링 대기 방식으로 우회 및 개선)
- 델리게이트 바인딩 및 해제로 인한 수명 주기 꼬임을 원천 차단하기 위해 **폴링(Polling) 대기 방식**을 설계 및 적용했습니다.
- 델리게이트에 직접 바인딩하여 콜백을 듣는 방식 대신, 비동기 파괴가 완료되었는지 여부를 매 프레임 `LobbyProvider->HasLocalSession()`을 통해 판별합니다.
- 매 0.1초마다 `FTSTicker`를 이용해 폴링 체크를 수행하다가, `HasLocalSession() == false`가 감지되면 대기 틱커를 정리하고 다음 틱에 다음 단계(`BeginSearchPhase` 혹은 `BeginCreatePhase`)로 안전하게 진입하도록 구현했습니다.
- 사용 후 틱커 핸들은 `ClearWaitLobbyTicker()` 및 소멸자에서 완벽히 `Remove`되므로 댕글링 포인터나 UAF 메모리 오염 걱정이 전혀 없는 완벽히 안전한 구조가 완성되었습니다.
