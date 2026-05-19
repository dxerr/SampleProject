# Bug Report: 멀티플레이 매칭 타임아웃 지연 및 SinglePlay 전환 크래시 수정

## 1. 이슈 개요 (Issue Summary)
- **증상 1 (매칭 타임아웃 지연)**: DataAsset에 `MaxWaitForPlayersSeconds`를 20초로 설정했으나, 실제 게임에서는 3분(180초)이 지나도록 갱신되지 않고 매칭 팝업이 멈추는 현상 발생.
- **증상 2 (SinglePlay 크래시)**: MultiPlay 매칭을 취소하고 곧바로 SinglePlay를 실행할 경우, `FExListenServerStrategy::FindAndJoinOrCreate` 람다 내부에서 `FString::operator=` 접근 중 메모리 충돌(memcpy)로 인한 크래시 발생.
- **증상 3 (EOS 경고 로그)**: `LogEOSRTC: TickTracker Ticks have been delayed` 경고가 반복해서 출력됨.

## 2. 원인 분석 (Root Cause Analysis)
### 2.1 매칭 타임아웃 지연
- `UExLobbyMatchViewModel::StartMultiPlay()`에서 DataAsset의 설정값을 가져올 때 `GetWorld()`가 `nullptr`을 반환하는 경우가 발생.
- ViewModel은 위젯이나 액터가 아니므로 특정 경로에서 자체적인 `GetWorld()`가 유효하지 않아 설정값을 가져오는 로직이 스킵됨.
- 이로 인해 구조체 기본값인 `60초`가 타임아웃으로 적용되었으며, 3회 재시도가 모두 도는 데 정확히 3분(60초 * 3)이 소요되어 무한 대기처럼 보였음.
- 또한 호스트 환경에서 `GetNamedSession`이 유효하지 않을 경우 타임아웃 체크를 아예 건너뛰는 예외 구멍이 존재했음.

### 2.2 SinglePlay 전환 시 메모리 크래시 (memcpy)
- 멀티플레이 대기열 등록(EOS API)은 비동기로 처리됨. 사용자가 취소 후 싱글플레이를 실행했을 때, 이전 람다 콜백들이 `OnCreateComplete` 멀티캐스트 델리게이트에 중첩되어 남아있었음.
- 로비 생성이 완료되는 순간 이전 멀티플레이 람다와 새로운 싱글플레이 람다가 동시에 실행되면서, `CurrentWaitConfig`의 String 데이터를 덮어씌우는 과정에서 충돌이 발생함.

### 2.3 EOS SDK Tick 지연 경고
- 에디터가 백그라운드로 내려가면(포커스 잃음) 언리얼 엔진의 "백그라운드에서 CPU 덜 사용" 옵션으로 인해 프레임이 3~5 FPS로 크게 떨어짐.
- 실시간 통신을 담당하는 EOS RTC 모듈이 엔진의 틱 주기가 늦어짐을 감지하고 발생시키는 단순 지연 경고로, 로직에는 악영향이 없는 정상 동작임.

## 3. 해결 및 조치 사항 (Resolution)
1. **타임아웃 설정 정상화**: 
   - ViewModel 내부에서 `GetWorld()` 대신 이미 유효성이 검증된 `CachedOnlineSubsystem->GetWorld()`를 호출하여 안정적으로 `MaxWaitForPlayersSeconds`(20초) 데이터를 로드하도록 수정.
2. **타임아웃 무한 루프 방지**: 
   - `CheckLobbyWaitConditions_Host` 내부에서 Session이 유효하지 않을 때 단순히 루프를 유지(`return true`)하는 것이 아니라, 즉시 타임아웃으로 간주해 대기를 파기하도록 수정.
3. **람다 중첩에 의한 크래시 해결**:
   - `FindAndJoinOrCreate` 함수에서 비동기 람다를 등록하기 전에 `LobbyProvider->OnFindComplete.Clear()` 등을 명시적으로 호출.
   - 이전 작업의 잔존 콜백을 완전히 소거하여 람다가 중복 실행되는 메모리 충돌을 차단.
4. **상태 로그 강화**: 
   - 타임아웃 발생 시 정확히 몇 초가 경과되어 취소되었는지 출력하는 로그(`UE_LOG`) 추가.

## 4. 결과 (Result)
- 크로스 플랫폼 환경에서 매칭 팝업의 [검색 시도] UI가 정확히 20초마다 갱신(1/3 -> 2/3 -> 3/3)되며, 모든 시도가 끝나면 매칭 실패 팝업이 정상 출력됨.
- 매치 취소 후 다른 모드로 전환하더라도 더 이상 람다 메모리 크래시가 발생하지 않음.
