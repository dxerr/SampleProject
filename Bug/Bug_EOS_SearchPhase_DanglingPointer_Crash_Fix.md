# Bug_EOS_SearchPhase_DanglingPointer_Crash_Fix.md

> 작성일: 2026-05-18
> 해결자: Antigravity
> 대상 모듈: `ExNetworkRuntime` (`FExListenServerStrategy`)

---

## 1. 이슈 개요 (Issue Overview)
- **현상**: Standalone 또는 PIE 환경에서 멀티플레이 매칭 진행 도중 창을 닫거나 월드가 전환될 때 (Deinitialize 시점), 혹은 매칭 검색 과정 중 예기치 않게 비정상 종료(Crash)가 발생함.
- **오류 메시지**: `Fatal Error: EXCEPTION_ACCESS_VIOLATION_READ / 0xffffffffffffffff`
- **핵심 콜스택**:
  ```text
  ExFrameWork  FExListenServerStrategy::BeginSearchPhase::2::<T>::operator() (ExListenServerStrategy.cpp:165)
  ExFrameWork  FExEOSLobbyProvider::HandleFindSessionsComplete (ExEOSLobbyProvider.cpp:268)
  ```

---

## 2. 세부 원인 분석 (Root Cause Analysis)

1. **지연 비동기 콜백 (Asynchronous Callback)의 성격**:
   - `LobbyProvider->FindLobbies()`를 실행하면 EOS SDK를 통해 비동기로 매치 세션 검색을 진행합니다.
   - 이때 검색 완료 시 결과를 통보받기 위해 `LobbyProvider->OnFindComplete` 델리게이트에 람다(Lambda) 콜백을 등록합니다.
   
2. **생명주기 불일치 및 댕글링 포인터 (Dangling Pointer)**:
   - 등록되는 람다 식은 `this`(`FExListenServerStrategy*` 생포인터)를 캡처하여 `WaitStartTime` 등 Strategy 멤버 변수에 접근합니다.
   - 하지만 매칭 검색 도중 창을 닫거나 게임 세션이 전환되면 `UExOnlineSubsystem::Deinitialize()`가 트리거되고 `ServerStrategy.Reset()`이 실행되어 **`FExListenServerStrategy` 인스턴스가 소멸**됩니다.
   - `FExListenServerStrategy`는 소멸되었지만, 비동기 호출 상태였던 `LobbyProvider`가 소멸되지 않고 백그라운드에서 실행되다가 완료된 순간 `HandleFindSessionsComplete`를 통해 `OnFindComplete.Broadcast()`를 쏘게 됩니다.
   - 이미 메모리에서 사라진(Dangling) 옛 Strategy 인스턴스의 람다 콜백이 실행되면서, `this->WaitStartTime`에 부적절하게 읽기 접근(`EXCEPTION_ACCESS_VIOLATION_READ / 0xffffffffffffffff`)을 시도하여 즉시 엔진 크래시가 유발되는 것이 핵심 원인이었습니다.

---

## 3. 해결 설계 및 조치 내용 (Resolution & Implementation)

- **Strategy 소멸자 안전 보강**:
  - `FExListenServerStrategy`의 소멸자(`~FExListenServerStrategy`)에 댕글링 포인터 차단 안전망을 강력하게 도입하였습니다.
  - 소멸자가 호출되는 즉시 `LobbyProvider`에 등록되어 있는 모든 델리게이트(`OnFindComplete`, `OnCreateComplete`, `OnJoinComplete`)를 강제로 `.Clear()` 처리하였습니다.
  - 이를 통해 비동기 검색 작업이 뒤늦게 끝나 `LobbyProvider`가 브로드캐스트를 시도하더라도, 파괴된 Strategy의 람다 함수로 실행 흐름이 전달되지 못하도록 원천 차단하였습니다.

### 3.1 ExListenServerStrategy.cpp 수정 내용
```cpp
FExListenServerStrategy::~FExListenServerStrategy()
{
	if (UpdateSessionHandle.IsValid())
	{
		// ...
		UpdateSessionHandle.Reset();
	}

	ClearWaitLobbyTicker();

	// [댕글링 포인터 크래시 방지] Strategy 소멸 시 LobbyProvider의 모든 델리게이트를 확실하게 정리
	if (LobbyProvider)
	{
		LobbyProvider->OnFindComplete.Clear();
		LobbyProvider->OnCreateComplete.Clear();
		LobbyProvider->OnJoinComplete.Clear();
	}

	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] 소멸됨 — Ticker 및 LobbyProvider 델리게이트 해제 완료."));
}
```

---

## 4. 최종 검증 (Verification)
- 소멸자 단에서의 델리게이트 청소 처리를 추가함으로써, 비동기 탐색 작업이 완료되기 전 게임 인스턴스 종료나 월드 트래블이 일어나도 댕글링 람다가 수행되지 않고 세션 파괴가 완벽하게 안전하게 처리됨을 확인하였습니다.
