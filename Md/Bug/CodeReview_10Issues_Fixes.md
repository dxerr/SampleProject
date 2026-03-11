# 코드 리뷰 기반 10가지 개선 및 버그 수정

## 개요
- **일자**: 2026-03-11
- **키워드**: Code Review, HUD 이중화, RemoveAllViewportWidgets, 델리게이트 중복 바인딩, 상대 경로 Include, 비동기 로딩, TSoftClassPtr, StreamableManager, GameMode Tick
- **목적**: 다른 AI(또는 리뷰어)로부터 전달받은 10가지 심각/경고/개선 코드 아키텍처 및 렌더링 결함을 수정하고 프로젝트의 안정성과 효율성을 높임.

## 문제점 및 해결 과정

### 1. [심각] HUD 생성 경로 이중화 (AExRunnerHUD vs ExPlayerControllerBase)
- **증상**: Experience 기반 `DefaultHUDLayout` 로딩과 `AExRunnerHUD`의 `BeginPlay()` 기반 생성이 동시에 일어나 위젯이 이중으로 뷰포트에 띄워짐.
- **해결**: `AExRunnerHUD::BeginPlay()`에서 `CreateWidget` 및 `AddToViewport` 호출 로직을 제거하고, Experience 시스템의 제어를 전적으로 신뢰하도록 수정함.

### 2. [심각] 파일 중간 `#include` 배치
- **증상**: `ExPlayerControllerBase.cpp`의 중간(115줄 부근)에 Include 헤더가 배치되어 빌드 복잡도 및 가독성을 크게 훼손.
- **해결**: 모든 `#include` 구문을 파일의 최상단부로 이동시켜 C++ 표준 코딩 컨벤션을 준수함.

### 3. [심각] `LogTemp` 전역 로그 카테고리 남용
- **증상**: 다수의 ExCore 클래스들이 모두 범용 `LogTemp`를 사용하여 디버깅 및 필터링이 어려움.
- **해결**: `LogExCoreGM`, `LogExCorePC`, `LogExExperience`, `LogExGameFlow`, `LogExRunnerHUD`, `LogExRunnerMatchVM` 등 각 모듈 및 책임에 맞는 `DEFINE_LOG_CATEGORY_STATIC`을 선언하여 치환 완료함.

### 4. [심각] `RemoveAllViewportWidgets` 남용
- **증상**: UI 재생성 시 `GEngine->GameViewport->RemoveAllViewportWidgets()`를 호출해 시스템이 관리하는 다른 위젯까지 파괴할 위험 존재.
- **해결**: 해당 구문을 지우고, Experience에서 지정하는 새로운 HUD Layout 위젯 인스턴스만을 개별 생성(추후 필요시 저장 후 Remove 처리)하도록 변경함.

### 5. [경고] 조기 `Match_Playing` 전이
- **증상**: `AExGameModeBase::HandleStartingNewPlayer_Implementation`에서 `SetMatchPhase(Playing)`으로 무조건 하드코딩 이동시켜, 대기나 `bAutoStartOnReady` 조건을 차단함.
- **해결**: 해당 로직을 제거하고 `CheckAndStartMatch()` 함수를 호출하여 정상적인 `bAutoStartOnReady` 조건 분기 처리에 따르도록 유도함.

### 6. [개선] Experience 비동기 데이터 로딩 시스템 누락
- **증상**: 하드 레퍼런스인 `TSubclassOf`가 사용되어 대량의 UI/에셋 로드시 로딩 히치 발생 우려. (TODO로 남아있었음)
- **해결**: `UExExperienceDefinition`의 프로퍼티를 `TSoftClassPtr`로 전부 교체. `ExExperienceManagerComponent.cpp`에 언리얼 `UAssetManager::GetStreamableManager().RequestAsyncLoad`를 도입하여 완벽한 비동기 백그라운드 로드 체계 및 콜백 이벤트 호출(`OnExperienceLoadComplete`)을 완성함.

### 7. [개선] 불필요한 `Tick` 활성화
- **증상**: `AExGameModeBase`의 `Tick` 함수 내용이 비어있음에도 생성자에서 `bStartWithTickEnabled = true`로 되어 자원을 불필요하게 소모함.
- **해결**: `PrimaryActorTick.bStartWithTickEnabled = false;`로 정정함.

### 8. [개선] 상태 전이 제약 제거 후 안전장치 부재
- **증상**: 자유로운 State 전이가 가능해졌으나 오작동 추적 로그가 부족함.
- **해결**: `SetMatchPhase` 함수 내부에 전환 발생 위치 및 이전->변경 후 상태를 출력하는 경고 로그를 보강하여 디버깅을 용이하게 함.

### 9. [개선] 상대 경로 Include (`ExRunnerMatchViewModel.cpp`)
- **증상**: `#include "../../GameStates/ExRunnerGameState.h"` 식의 코드가 있어 모듈 복사/이동 시 오류 위험 존재.
- **해결**: `ExRunnerPlayRuntime.Build.cs` 내 `PublicIncludePaths`에 `ModuleDirectory`를 명시적으로 등록하여 `#include "GameStates/ExRunnerGameState.h"`와 같이 절대 경로 형태로 깔끔하게 동작하도록 엔진 빌드 파이프라인을 교정함.

### 10. [개선] 델리게이트 중복 바인딩 위험성
- **증상**: `AExPlayerControllerBase`의 `ReceivedPlayer` 등에서 로딩 콜백을 등록할 때 중첩 등록될 여지가 있음.
- **해결**: `AddUObject` 수행 전 `RemoveAll(this)`를 선행 호출하여 단일 인스턴스의 다중 콜백 스택 증식을 방지함.

## 결과
모든 수정 사항 반영 이후 1분 30초 내외의 컴파일 후 **Succeeded(0 에러)** 상태를 기록함. 앞으로 신규 Experience DataAsset 적용 시 비동기 로딩을 통한 UI 로드 등 히치 없는 프레임 및 상태관리가 가능해짐.
