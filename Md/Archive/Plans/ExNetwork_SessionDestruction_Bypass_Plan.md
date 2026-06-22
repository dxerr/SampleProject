# [구현 계획서] 커스텀 GameSession을 통한 EOS 세션 파괴 버그 모듈 레벨 우회 해결안

> **목적**: 로비 인원이 2/2명으로 채워져 게임 맵(`L_ExRunnerTest`)으로 이동할 때, 엔진의 자동 로그인(`AutoLogin`) 시도로 인해 기존에 정상 기동 중이던 디바이스 ID 로그인 세션이 파괴되어 접속이 끊기는 현상을 완벽히 차단한다.
> **해결 방법**: 외부 플러그인을 수정하는 대신 프로젝트의 `ExCore` 모듈에 커스텀 `AExGameSession` 클래스를 도입하여 파괴적인 `ProcessAutoLogin` 동작을 안전하게 우회(Bypass)시킨다.
> **상태**: 설계 완료 — 주인님 승인 완료

---

## 1. 개요 및 배경 (Background)

주인님, 본 계획서는 로비 정원(2/2) 충족 후 인게임 맵으로 전환될 때 로컬 유저가 로그아웃되고 로비가 파괴되는 치명적인 네트워크 버그를 해결하기 위한 문서입니다. 

### 1.1 현상과 원인
1. **정상 단계**: 게임 기동 시 `ExOnlineSubsystem`이 디바이스 ID를 통해 성공적으로 익명 로그인을 마쳐 로컬 고유 ID(`UniqueNetId`)를 보유하고 로비를 지탱함.
2. **트래블 발생**: 로비에 인원이 모두 차면 호스트가 `ServerTravel`을 수행하여 게임 맵(`L_ExRunnerTest`)으로 전환을 개시함.
3. **엔진의 자동 로그인 기동**: 새 맵의 `AGameModeBase::InitGame`이 실행되면서 엔진 레벨에서 `GameSession->ProcessAutoLogin()`을 자동으로 호출함.
4. **플랫폼 로그인 실패**: PC 개발 환경 등 Steam/Epic App이 켜져 있지 않은 stand-alone 모드에서는 플랫폼 토큰 획득에 실패하여 `FUserManagerEOS::AutoLogin`이 실패 콜백을 타게 됨.
5. **기존 세션 파괴**: 플러그인은 자동 로그인 실패 처리를 하면서 기존에 활성화 중이던 `LocalUsers[0]`의 `UniqueNetId` 정보를 완전하게 지워버리는(`RemoveLocalUser`) 파괴적인 버그를 일으킴.
6. **로비 세션 폭파**: 이로 인해 로컬 유저가 강제 로그아웃되고, 로비 매니저가 이를 감지하여 전체 커넥션을 닫아 모든 클라이언트가 이탈함.

### 1.2 방법 B 해결책 (커스텀 GameSession 구현)
* 엔진의 `AGameSession`을 직접 상속받는 `AExGameSession` 클래스를 프로젝트 `ExCore` 모듈에 새롭게 추가합니다.
* `ProcessAutoLogin()`을 재정의(Override)하여 중복으로 자동 로그인이 실행되지 않고 즉시 성공 처리를 뱉어내도록 하여, 기존 EOS 세션을 완벽하게 안전 구역에 보존합니다.
* 이를 통해 외부 플러그인(`OnlineSubsystemEOS`)을 전혀 수정하지 않고 정석적인 구조로 문제를 깔끔하게 해결합니다.

---

## 2. 작업 범위 (Scope)

### 2.1 신규 파일 추가 (`ExCoreRuntime`)
- [NEW] [ExGameSession.h](file:///c:/wz/ExFrameWork/Plugins/GameFeatures/ExCore/Source/ExCoreRuntime/GameModes/ExGameSession.h) — 커스텀 게임 세션 선언
- [NEW] [ExGameSession.cpp](file:///c:/wz/ExFrameWork/Plugins/GameFeatures/ExCore/Source/ExCoreRuntime/GameModes/ExGameSession.cpp) — 중복 로그인을 차단하는 `ProcessAutoLogin` 재정의

### 2.2 기존 파일 수정 (`ExCoreRuntime`)
- [MODIFY] [ExGameModeBase.cpp](file:///c:/wz/ExFrameWork/Plugins/GameFeatures/ExCore/Source/ExCoreRuntime/GameModes/ExGameModeBase.cpp) — 기본 `GameSessionClass`를 커스텀 `AExGameSession`으로 지정하도록 생성자 수정

---

## 3. 세부 파일 설계 명세

### 3.1 `ExGameSession.h` [NEW]
```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameSession.h"
#include "ExGameSession.generated.h"

/**
 * ExCore의 커스텀 게임 세션 클래스입니다.
 * 맵 전환 시 불필요한 자동 로그인을 방지하여 기존 로그인 세션을 보호합니다.
 */
UCLASS()
class EXCORERUNTIME_API AExGameSession : public AGameSession
{
	GENERATED_BODY()

public:
	/** 
	 * 자동 로그인 프로세스를 재정의하여 우회시킵니다.
	 */
	virtual bool ProcessAutoLogin() override;
};
```

### 3.2 `ExGameSession.cpp` [NEW]
```cpp
#include "GameModes/ExGameSession.h"
#include "Net/OnlineEngineInterface.h"
#include "Engine/World.h"

bool AExGameSession::ProcessAutoLogin()
{
	// 주인님! 이미 UI 로그인 단계를 거쳐 디바이스 ID 등으로 안전하게 로그인되어 있는 경우,
	// 맵 로딩 시 엔진이 이를 인지하지 못하고 중복 자동 로그인을 시도해 기존 EOS 로그인 세션을 
	// 파괴(RemoveLocalUser)하는 심각한 현상을 원천 방지하기 위해 이 과정을 안전하게 건너뜁니다.
	UE_LOG(LogGameSession, Log, TEXT("[AExGameSession] 주인님, 중복 자동 로그인 요청(ProcessAutoLogin)을 안전하게 차단하여 기존 EOS 세션을 전적으로 보존합니다."));
	
	// 자동 로그인이 비동기로 돌지 않고 즉시 성공한 것처럼 true를 리턴하여 엔진의 InitGame 흐름을 막힘없이 유지합니다.
	return true;
}
```

### 3.3 `ExGameModeBase.cpp` [MODIFY]
```cpp
// 생성자 부분 수정
#include "GameModes/ExGameSession.h" // 헤더 추가

AExGameModeBase::AExGameModeBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bUseSeamlessTravel = true;
	bAutoStartOnReady = false;

	// 주인님, 커스텀 게임 세션 클래스로 지정하여 자동 로그인 오동작을 프로젝트 레벨에서 우회합니다.
	GameSessionClass = AExGameSession::StaticClass();
}
```

---

## 4. 검증 계획 (Verification Plan)

### 4.1 컴파일 및 빌드 검증
- 수정한 C++ 코드들이 `ExCore` 모듈 컴파일 시 에러 없이 빌드 완료되는지 확인합니다.

### 4.2 인게임 매칭 및 트래블 런타임 검증
1. 호스트와 클라이언트 2인 멀티플레이어 환경 구동.
2. 로비에서 정원이 2/2 가 찰 때까지 대기 및 매칭 완료 유도.
3. 호스트가 인게임 맵(`L_ExRunnerTest`)으로 트래블 시도.
4. 호스트 서버 로그에 다음이 출력되는지 확인:
   `[AExGameSession] 주인님, 중복 자동 로그인 요청(ProcessAutoLogin)을 안전하게 차단하여 기존 EOS 세션을 전적으로 보존합니다.`
5. 두 플레이어 모두 접속이 끊기거나 로그아웃되지 않고 정상적으로 플레이어 스폰 및 매칭 인게임 맵으로 로딩이 완료되는지 시각적으로 확인합니다.

### 4.3 버그 관리 이력 작성
- 본 문제의 증상, 원인, 해결 결과를 `/Bug` 디렉터리에 `Bug_EOS_SessionDestruction_Bypass_Fix.md` 파일로 작성하여 아카이빙합니다.

---
주인님, 이상으로 프로젝트 모듈 레벨에서 해결하는 완벽한 구현 설계도를 완성하여 기록해 두었습니다, 주인님. 즉시 구현을 시작하여 최고의 결과를 보고드리겠습니다, 주인님!
