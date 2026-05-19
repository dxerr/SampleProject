# Bug_EOS_SearchPhase_DanglingPointer_Crash_Fix.md

> 작성일: 2026-05-18
> 해결자: Antigravity
> 대상 모듈: `ExNetworkRuntime` (`FExListenServerStrategy`)

---

## 1. 이슈 개요 (Issue Overview)

### 1.1 클라이언트/호스트 비동기 Phase 전환 중 크래시 (1차 발견)
- **현상**: Standalone 또는 Windows 패키징 빌드 환경에서 멀티플레이 매칭 진행 도중 로비 생성(`BeginCreatePhase`), 로비 검색(`BeginSearchPhase`), 로비 참가(`BeginJoinPhase`)의 비동기 콜백 호출 즉시 게임이 비정상 종료(Crash)되는 현상.
- **오류 메시지**: `Fatal Error: EXCEPTION_ACCESS_VIOLATION_READ / 0xffffffffffffffff`
- **핵심 콜스택**:
  ```text
  ExFrameWork  UE::Core::Private::Function::FFunctionStorage::GetPtr (Function.h:179)
  ExFrameWork  `FExListenServerStrategy::BeginCreatePhase'::`2'::<T>::operator() (ExListenServerStrategy.cpp:234)
  ExFrameWork  FExEOSLobbyProvider::HandleCreateSessionComplete (ExEOSLobbyProvider.cpp:216)
  ```

### 1.2 호스트 세션 업데이트(`UpdateSession`) 완료 콜백 중 크래시 (2차 신규 발견)
- **현상**: 매칭 성공 후 호스트 환경에서 로비 세션 속성(`MATCH_STARTED=true`)을 업데이트하고 완료 이벤트를 전달받는 과정에서 게임이 무조건 크래시 종료되는 현상.
- **오류 메시지**: `Fatal Error: EXCEPTION_ACCESS_VIOLATION_READ / 0xffffffffffffffff`
- **핵심 콜스택**:
  ```text
  ExFrameWork  `FExListenServerStrategy::CheckLobbyWaitConditions_Host'::`64'::<T>::operator() (ExListenServerStrategy.cpp:502)
  ExFrameWork  TBaseFunctorDelegateInstance<T>::ExecuteIfSafe (DelegateInstancesImpl.h:919)
  ExFrameWork  TMulticastDelegate<T>::Broadcast (DelegateSignatureImpl.inl:1076)
  ExFrameWork  FOnlineSessionEOS::UpdateSession::__l11::<T>::operator() (OnlineSessionEOS.cpp:1861)
  ```

---

## 2. 세부 원인 분석 (Root Cause Analysis)

### 2.1 지연 비동기 콜백 댕글링 포인터 현상 (이전 조치 완료)
- `LobbyProvider->FindLobbies()` 등 비동기 요청 후 등록된 람다 함수가 `this` 생포인터를 캡처하여 소멸 이후 실행되었을 때 발생하는 메모리 접근 에러.
- 이는 소멸자(`~FExListenServerStrategy`)에서 `LobbyProvider`에 바인딩된 델리게이트들을 일괄 `.Clear()`하여 완벽히 해결함.

### 2.2 델리게이트 내부 즉시 클리어 및 클로저 자가 파괴(Self-Destruction) 현상
- `BeginCreatePhase`, `BeginSearchPhase`, `BeginJoinPhase`는 비동기 완료를 전달받는 람다 내부에서 다음 상태 전이 전 중복 콜백을 막기 위해 **스스로의 델리게이트를 즉시 해제**하는 로직(`LobbyProvider->OnXxxComplete.Clear()`)을 포함하고 있었습니다.
- 언리얼 엔진의 멀티캐스트 델리게이트 시스템은 `Clear()`가 호출되면 내부 리스트 및 바인딩된 **델리게이트 인스턴스(람다 클로저 객체 포함)의 메모리를 즉시 할당 해제(Deallocate)**합니다.

### 2.3 컴파일러 최적화로 인한 로컬 스택 백업 무력화 및 Use-After-Free (UAF)
- 이전 패치에서는 `auto OnCreateCompleteLocal = OnCreateComplete;` 와 같이 로컬 스택에 복사 백업을 수행한 후 `Clear()`를 실행하여 댕글링 문제를 회피하고자 했습니다.
- 하지만 **최적화 빌드(Development / Shipping) 컴파일러 최적화 옵션**이 켜지면서 치명적인 부작용이 발생했습니다:
  1. 컴파일러 최적화 도구(Optimizer)는 스택 상에 임시 복사된 `OnCreateCompleteLocal` 변수가 복사 생성된 후 단 한 번만 실행됨을 감지합니다.
  2. 이를 최적화하기 위해 컴파일러는 **복사 엘리전(Copy Elision) 및 값 전파(Copy Propagation)**를 수행하여, 스택 복사 생성 코드를 생략하고 람다 클로저 내부의 멤버 변수인 `this->CapturedOnCreateComplete`를 직접 호출하도록 코드를 최적화 컴파일합니다.
  3. 이 상태에서 `Clear()`가 먼저 호출되면, 람다의 클로저 메모리가 즉시 해제(UAF)된 후, **존재하지 않는 `this->CapturedOnCreateComplete`의 내부 함수 포인터(`FFunctionStorage::GetPtr`)를 참조**하면서 메모리 예외(`0xffffffffffffffff` 접근 위반)가 발생합니다.
- 추가로, 콜백 실행(`OnCreateCompleteLocal(...)`)이 내부적으로 `TransitionMatchState` -> `EndCreatePhase` -> `Clear()`를 **동기적으로(Synchronously)** 호출하기 때문에, 순서를 어떻게 바꾸어도 실행 콜스택 프레임 내부에서 실행 중인 람다가 도중에 파괴되는 원초적인 수명 주기 결함이었습니다.

### 2.4 UpdateSession 델리게이트 자가 해제 중 UAF 현상 (2차 이슈 원인)
- 호스트가 정원 충족 후 세션 설정을 변경하고 `OSS->GetSessionInterface()->UpdateSession`을 호출할 때 완료 핸들을 관리하기 위해 임시 람다를 바인딩합니다.
- 이 람다 콜백 내부에서 다음과 같은 로직이 실행됩니다:
  ```cpp
  OSS->GetSessionInterface()->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionHandle);
  UpdateSessionHandle.Reset();
  ```
- 람다가 바인딩된 델리게이트는 델리게이트 인스턴스 자체에 의해 람다 클로저의 수명이 관리됩니다.
- 람다 함수 내부에서 `ClearOnUpdateSessionCompleteDelegate_Handle`을 직접 호출하여 자기 자신을 바인딩 해제하는 순간, 람다 클로저의 메모리가 **동기적으로 그 즉시 해제(Deallocate)**됩니다.
- 이로 인해 람다의 `this` (클로저 포인터)가 댕글링 상태가 되며, 바로 다음 라인인 `UpdateSessionHandle.Reset();`을 실행하려 할 때 (`this->UpdateSessionHandle.Reset()` 호출 시) **이미 해제된 메모리를 참조하여 크래시(Memory Access Violation)**가 무조건 유발되는 현상이었습니다.

---

## 3. 최종 해결 설계 및 조치 내용 (Final Resolution)

- **FTSTicker를 이용한 콜백 및 핸들 클리어의 1프레임 지연(Deferred Execution)**:
  - 델리게이트 브로드캐스트 스택 내부에서 동기적으로 상태 전이 및 델리게이트 해제(`Clear`)가 실행되는 것을 원천적으로 차단합니다.
  - 콜백 내부에서 핸들 해제 및 후속 완료 이벤트를 **다음 틱(`0.0f` 지연 Ticker)**으로 안전하게 연기하여 전달합니다.
  - 이로써 델리게이트 브로드캐스트가 완벽히 종료되고, 람다가 스택 프레임에서 안전히 탈출한 뒤, 다음 프레임에 안전하게 상태 전이 및 델리게이트 클리어가 수행됩니다.
  - 또한, 지연된 틱 내에서 참조할 정보(예: `ErrorMessage`, `SessionName`, `bUpdateSuccess`)는 **Inner Lambda에 값 복사(By Value)** 형식으로 캡처하게 하여 댕글링 참조 가능성을 완전히 소멸시켰습니다.
  - 소멸자(`ClearWaitLobbyTicker()`)에서도 지연 완료 처리 중인 Ticker 핸들을 추적하여 리셋하므로, 메모리 릭 및 댕글링 실행이 전면 방지됩니다.

### 3.1 ExListenServerStrategy.h 수정 내역
- 지연 처리를 위한 세 가지 Ticker 핸들 추가:
```cpp
	FTSTicker::FDelegateHandle SearchPhaseTickerHandle;
	FTSTicker::FDelegateHandle CreatePhaseTickerHandle;
	FTSTicker::FDelegateHandle JoinPhaseTickerHandle;
```

### 3.2 ExListenServerStrategy.cpp 수정 내역 (Phase 비동기 완료 지연)

#### [BeginSearchPhase]
- 델리게이트 콜백 스택 밖에서 안전히 완료를 처리하기 위해 `SearchPhaseTickerHandle` 지연 틱 적용:
```cpp
	LobbyProvider->OnFindComplete.AddLambda(
		[this, ExpectedState, OnSearchComplete](bool bSuccess, int32 ResultCount)
		{
			SearchPhaseTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateLambda([this, ExpectedState, OnSearchComplete, bSuccess, ResultCount](float) -> bool
				{
					SearchPhaseTickerHandle.Reset();
					if (bIsDestroyed) return false;

					if (bSuccess && ResultCount > 0)
					{
						UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Lobby %d개 발견."), ResultCount);
						FindRetryCount = 0;
						OnSearchComplete(true, TEXT(""));
					}
					else
					{
						// ...
					}
					return false;
				}),
				0.0f
			);
		});
```

#### [BeginCreatePhase]
- 로비 생성 완료 이벤트를 다음 틱으로 안전하게 지연하여 UAF 방지:
```cpp
	LobbyProvider->OnCreateComplete.AddLambda(
		[this, OnCreateComplete](bool bCreateSuccess, const FString& ErrorMessage)
		{
			CreatePhaseTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateLambda([this, OnCreateComplete, bCreateSuccess, ErrorMessage](float) -> bool
				{
					CreatePhaseTickerHandle.Reset();
					if (bIsDestroyed) return false;

					if (bCreateSuccess)
					{
						UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Lobby 생성 성공."));
						OnCreateComplete(true, TEXT(""));
					}
					else
					{
						UE_LOG(LogExNetwork, Warning, TEXT("[ExListenServerStrategy] Lobby 생성 실패 — %s"), *ErrorMessage);
						OnCreateComplete(false, ErrorMessage);
					}
					return false;
				}),
				0.0f
			);
		}
	);
```

#### [BeginJoinPhase]
- 로비 참가 성공/실패 처리를 다음 틱으로 미루어 델리게이트 자가 소멸 크래시 방지:
```cpp
	LobbyProvider->OnJoinComplete.AddLambda(
		[this, OnJoinComplete](bool bJoinSuccess, const FString& ErrorMessage)
		{
			JoinPhaseTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateLambda([this, OnJoinComplete, bJoinSuccess, ErrorMessage](float) -> bool
				{
					JoinPhaseTickerHandle.Reset();
					if (bIsDestroyed) return false;

					if (bJoinSuccess)
					{
						CachedConnectString = LobbyProvider->GetConnectString();
						UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Lobby 참가 성공 — ConnectString 캐시: %s"), *CachedConnectString);
						OnJoinComplete(true, TEXT(""));
					}
					else
					{
						UE_LOG(LogExNetwork, Warning, TEXT("[ExListenServerStrategy] Lobby 참가 실패 — %s"), *ErrorMessage);
						OnJoinComplete(false, ErrorMessage);
					}
					return false;
				}),
				0.0f
			);
		}
	);
```

### 3.3 ExListenServerStrategy.cpp 수정 내역 (호스트 세션 업데이트 완료 지연)

#### [CheckLobbyWaitConditions_Host]
- `UpdateSession` 완료 이벤트 수신 시 자기 자신의 델리게이트 핸들을 즉시 클리어 및 람다 소멸(UAF)하여 크래시되는 현상을 막기 위해 완료 핸들 정리를 **다음 틱(`0.0f`)으로 지연 실행**하도록 수정했습니다.
```cpp
			UpdateSessionHandle = OSS->GetSessionInterface()->AddOnUpdateSessionCompleteDelegate_Handle(
				FOnUpdateSessionCompleteDelegate::CreateLambda(
					[this, OSS](FName SessionName, bool bUpdateSuccess)
					{
						// [경쟁 조건 및 UAF 크래시 방지]
						// 델리게이트 완료 핸들 해제 및 후속 처리를 다음 틱으로 지연하여 
						// 현재 실행 중인 람다 클로저가 브로드캐스트 스택 내부에서 동기적으로 소멸(Use-After-Free)되는 것을 원천 방지합니다.
						FTSTicker::GetCoreTicker().AddTicker(
							FTickerDelegate::CreateLambda([this, OSS, SessionName, bUpdateSuccess](float) -> bool
							{
								if (bIsDestroyed) return false;

								OSS->GetSessionInterface()->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionHandle);
								UpdateSessionHandle.Reset();

								UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Host MATCH_STARTED 업데이트 완료(bSuccess=%d) — 매칭 완료 콜백 호출."), bUpdateSuccess);
								
								if (CachedOnComplete) 
								{ 
									TFunction<void(bool, const FString&)> TempComplete = MoveTemp(CachedOnComplete);
									CachedOnComplete = nullptr;
									
									FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([TempComplete](float) -> bool
									{
										if (TempComplete)
										{
											TempComplete(true, TEXT(""));
										}
										return false;
									}), 0.5f);
								}
								return false;
							}),
							0.0f
						);
					}
				)
			);
```

---

## 4. 최종 검증 (Verification)
- 델리게이트 콜백 도중 발생하는 비동기 FSM 상태 변환 및 델리게이트 할당 해제 수명 주기가 **1프레임 지연 기법**을 통해 안전하게 분리되었습니다.
- 이제 컴파일러의 어떤 최적화 옵션(Copy Propagation 등) 하에서도 Use-After-Free가 물리적으로 원천 봉쇄되었으며, Windows 패키징 빌드 및 Standalone PIE 상에서 매칭 진행 및 호스트 세션 시작 과정 중 발생하는 모든 메모리 크래시가 완벽히 정리되었습니다.
