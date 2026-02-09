# ExFrameWork 개발 가이드라인

이 문서는 ExFrameWork 프로젝트의 개발 규칙과 컨벤션을 정의합니다. 새로운 규칙이 생길 때마다 지속적으로 업데이트됩니다.

## 1. 코딩 규칙 (Coding Standards)

### 1.1 접근 지정자 (Access Specifiers)
- **Public/Private 구분 없음**: 소스 코드 작성 시 `public`, `private`, `protected` 접근 지정자를 엄격하게 구분하지 않고, 기본적으로 접근이 용이하도록 작성합니다. (특별한 보안이나 캡슐화가 필수적인 경우 제외)

### 1.2 명명 규칙 (Naming Conventions)
- **클래스/구조체 접두사**: 새로운 클래스나 구조체를 선언할 때는 반드시 **"Ex"** 접두사를 사용합니다.
    - `UObject` 상속: `UEx...` (예: `UExMyObject`)
    - `AActor` 상속: `AEx...` (예: `AExCharacter`)
    - 구조체: `FEx...` (예: `FExData`)
    - 열거형(Enum): `EEx...` (예: `EExStateType`)
    - 인터페이스(Interface): `IEx...` (예: `IExInteractable`)
- **변수 및 함수**: PascalCase를 사용합니다.
    - **Boolean 변수**: 반드시 `b` 접두사를 붙입니다. (예: `bIsReady`)
- **에셋 명명 규칙**: 에셋 유형별 접두사를 준수합니다.
    - Blueprint Class: `BP_Ex[Name]`
    - Widget Blueprint: `WBP_Ex[Name]`
    - Data Asset: `DA_Ex[Name]`
    - Data Table: `DT_Ex[Name]`

### 1.3 데이터 드리븐 설계 (Data-Driven Design)
- **하드코딩 배제**: 매직 넘버, 문자열 리터럴 등 하드코딩된 값은 최대한 피합니다.
- **UPROPERTY 노출**: 조정 가능한 값들은 `UPROPERTY(EditAnywhere)` 또는 `UPROPERTY(EditDefaultsOnly)`로 에디터에 노출하여 데이터 드리븐 방식으로 관리합니다.
- **설정 파일 활용**: 필요 시 데이터 테이블, 커브, INI 파일 등을 활용하여 런타임 또는 에디터에서 값을 변경할 수 있도록 합니다.

### 1.4 단일 책임 원칙 (Single Responsibility Principle)
- **하나의 클래스/함수는 하나의 책임만**: 각 클래스와 함수는 명확하게 정의된 단일 책임만을 가지도록 설계합니다.
- **기능 분리**: 여러 기능이 혼재된 경우, 별도의 컴포넌트나 헬퍼 클래스로 분리합니다.
- **유지보수성 향상**: 변경의 이유가 하나만 존재하도록 하여 코드 수정 시 영향 범위를 최소화합니다.

### 1.5 폴더 구조 규칙 (Folder Structure)
- **Struct 폴더**: 구조체(`USTRUCT`) 정의는 한눈에 파악하기 쉽도록 `Struct/` 폴더 하위에 배치합니다.
    - 참조하는 클래스의 Hierarchy 구조를 최대한 따릅니다.
    - 예: `Source/ExFrameWork/Struct/Modes/FExGameModeData.h`
- **Data 폴더**: 데이터 애셋(`UDataAsset`) 및 관련 구조체 정의는 `Data/` 폴더 하위에 배치합니다.
    - 참조하는 클래스의 Hierarchy 구조를 최대한 따릅니다.
    - 예: `Source/ExFrameWork/Data/Characters/UExCharacterData.h`
- **Util 폴더**: 유틸리티성 함수들(Static 함수, Math 함수, Find 함수 등)은 `Util/` 폴더 하위에 배치합니다.
    - 각 속성에 맞는 하위 폴더를 생성하여 정의합니다.
    - 예: `Source/ExFrameWork/Util/Math/ExMathLibrary.h`
    - 예: `Source/ExFrameWork/Util/Find/ExFindHelper.h`

### 1.6 주석 및 문서화 규칙 (Comment & Documentation)
- **클래스/함수/구조체 주석**: 모든 클래스, 함수, 구조체에는 역할과 사용법을 명확히 설명하는 주석을 작성합니다.
    - Unreal Engine 표준 또는 Doxygen 스타일(`/** ... */`) 권장
    - 예시:
      ```cpp
      /**
       * 캐릭터의 체력을 반환합니다.
       * @return 현재 체력 값
       */
      float GetHealth() const;
      ```
- **파일 헤더 주석**: 모든 소스 파일 상단에는 파일 목적, 작성자, 생성일, 변경 이력 등을 포함한 헤더 주석을 작성합니다.
- **TODO/FIXME 주석**: 추가 구현이 필요한 부분이나 수정이 필요한 부분에는 `TODO`, `FIXME` 주석을 명확히 남깁니다.
- **한글/영문 혼용**: 주석은 한글 또는 영문 모두 허용하나, 팀 내 일관성을 유지합니다.

### 1.7 언리얼 엔진 베스트 프랙티스 (UE5 Best Practices)
- **TObjectPtr**: 헤더 파일의 멤버 변수 선언 시 Raw Pointer(`*`) 대신 `TObjectPtr<T>`를 사용하는 것을 권장합니다 (UE5 표준).
- **로그 매크로**: `LogTemp` 대신 프로젝트 전용 로그 카테고리(`LogExFrameWork`)를 정의하여 사용합니다.
- **검증(Assertions)**:
    - `check()`: 치명적인 오류, 개발 중 즉시 중지되어야 하는 경우.
    - `ensure()`: 예외 상황이지만 실행은 계속되어야 하는 경우 (최초 1회만 에디터에 알림).

### 1.8 네트워크 및 멀티플레이어 규칙 (Networking & Multiplayer)
- **서버 권한(Authority) 고려**: 모든 게임 로직 작성 시 데디케이티드 서버(Dedicated Server) 환경을 기본으로 고려합니다.
    - 주요 상태 변경이나 중요한 로직은 반드시 서버 권한(`HasAuthority()`)을 확인하고 실행해야 합니다.
    - 클라이언트는 서버의 상태를 복제받아 표현(Presentation)하는 역할에 집중합니다.
    - RPC(Remote Procedure Call) 사용 시 보안과 대역폭을 고려하여 최소화합니다.

### 1.9 헤더 파일 구조 (Header File Structure)
- **Generated Header 위치**: 언리얼 헤더 툴(UHT)의 정상적인 동작을 위해 `#include "ExMyClass.generated.h"` 구문은 반드시 헤더 파일의 **가장 마지막** `#include` 문으로 위치해야 합니다.
    - 예시:
      ```cpp
      #include "CoreMinimal.h"
      #include "GameFramework/Actor.h"
      // 다른 헤더들...
      #include "ExMyClass.generated.h" // 반드시 마지막!
      ```

### 1.10 생성자 및 초기화 규칙 (Constructors & Initialization)
- **생성자 내 NewObject 금지**: 클래스 생성자(`Constructor`) 내부에서는 `NewObject<>`를 호출하거나 물리/렌더링 상태를 갱신하는 함수(`SetBoxExtent` 등)를 호출하면 안 됩니다.
    - 대신 `CreateDefaultSubobject<>`를 사용하거나, 값만 설정하는 `Init...` 계열 함수(`InitBoxExtent` 등)를 사용해야 합니다.
    - **이유**: 생성자 단계에서 불완전한 객체가 엔진 시스템(Physics 등)과 상호작용하려 하면 `NewObject with empty name` 등의 치명적 크래시가 발생합니다.


## 2. 문서 관리


### 2.1 문서 폴더 구조 (Folder Structure)
- **Architecture/**: 시스템 아키텍처, 분석 보고서, 설계 문서 등을 보관합니다.
    - **ExCore/**: 핵심 프레임워크 관련 (장르 무관 범용 시스템)
        - 예: `Mover_System_Analysis.md`, `DataDrivenCVars_Analysis.md`
    - **ExRunnerPlay/**: 러너 게임 특화 기능 관련
        - 예: `ExRunner_System_Architecture.md`, `Issue_Obstacle_Sync_Report.md`
- **Guides/**: 사용 가이드, 튜토리얼, 셋업 가이드 등을 보관합니다.
    - **ExCore/**: 핵심 프레임워크 가이드
        - 예: `ExCore_GameplayTag_EventSystem_Guide.md`
    - **ExRunnerPlay/**: 러너 게임 관련 가이드
        - 예: `Climb_Sync_Guide.md`, `CurvedWorld_Runner_Setup_Guide.md`
    - **Common/**: 엔진, 도구, 범용 설정 가이드 (특정 모듈에 종속되지 않음)
        - 예: `MotionMatching_Guide_KR.md`, `PythonBridge_Documentation.md`
- **Bug/**: 개발 중 발생한 크리티컬 이슈와 해결 방법을 기록합니다.
    - 파일명: `[이슈키워드]_[원인].md` (예: `Constructor_Crash_NewObject.md`)
    - 비슷한 이슈 발생 시 우선 검색하여 해결책을 찾습니다.
- **Legacy/**: 오래된 보고서, 더 이상 유효하지 않지만 참고용으로 남겨둔 문서들을 보관합니다.
    - 예: `Legacy_Spawner_Implementation_Report.md`
- **Root**: `ExFrameWork_Guidelines.md`와 같은 프로젝트 전반에 걸친 핵심 기준 문서는 루트에 위치합니다.

### 2.2 관리 규칙
- 이 파일(`ExFrameWork_Guidelines.md`)은 프로젝트의 살아있는 규칙 문서로 관리됩니다.
- 새로운 규칙이나 변경 사항이 발생하면 즉시 이 문서를 업데이트해야 합니다.

## 3. 개발 프로세스 (Development Process)

### 3.1 보고 및 승인 절차 (Report & Approval)
- **주요 변경 사항 사전 보고**: 중요한 기능 구현이나 구조적 수정이 필요한 경우, 구현 전 반드시 **보고서(Plan/Report)** 를 작성합니다.
    - 내용: 문제 정의, 원인 분석, 해결 계획, 예상되는 사이드 이펙트 등.
- **승인 후 구현**: 작성된 보고서 및 계획에 대해 사용자의 승인을 받은 후 코드 구현을 시작합니다.
- **예외**: 사소한 오타 수정이나 명백한 버그 픽스(단일 라인 수정 등)는 유연하게 처리할 수 있으나, 사후에라도 반드시 보고해야 합니다.

### 3.2 버전 관리 규칙 (Git Workflow)
- **Git 커밋/푸시 통제**: 
    1. 코드 구현 및 로컬 테스트가 완전히 끝났더라도 **사용자의 명시적인 요청**이 있기 전까지는 `git commit` 및 `git push`를 수행하지 않습니다.
    2. 모든 테스트가 완료된 후 사용자에게 "테스트 완료, 커밋 대기 중"임을 보고합니다.
    3. 사용자의 승인/요청이 확인되면 커밋 및 푸시를 진행합니다.
- **커밋 메시지**: 변경 내용을 명확하게 알 수 있도록 [Type]: [Subject] 형식을 준수하고, 본문에 상세 내용을 리스트업합니다.

### 3.3 예외 상황 보고
- **진행 중 문제 발견 시**: 계획된 작업 도중 예상치 못한 문제나 누락된 구현이 발견되면, 즉시 작업을 일시 중단하고 사용자에게 상황을 보고합니다. 임의로 해결 후 통보하지 않습니다.

## 4. 아키텍처 및 모듈화 규칙 (Architecture & Modularization)

### 4.1 Core와 Feature 모듈 분리 (Core vs Feature)
- **Core 모듈의 역할**: `ExCore`와 같은 Core 모듈은 특정 게임 장르(예: Runner, FPS, RPG)에 종속되지 않는 **범용 프레임워크 기능**만을 포함해야 합니다.
    - 예: 비주얼 액터 관리, 기본 컨테이너 폰, 유틸리티 함수, 공통 인터페이스 등.
- **로직 구현 시 판단 기준**: 기능 구현 전 반드시 해당 기능의 성격을 판단하여 적절한 모듈에 배치해야 합니다.
    - **"이 로직이 러너(Runner) 게임에만 필요한가?"** -> **`ExRunnerPlay` (Feature Plugin)** 에 구현.
    - **"이 로직이 다른 장르(예: RPG)에서도 쓰일 수 있는가?"** -> **`ExCore` (Framework)** 에 구현.
- **의존성 방향 (Dependency Direction)**: Feature 모듈은 Core 모듈을 참조할 수 있지만, **Core 모듈은 절대로 Feature 모듈을 참조해서는 안 됩니다.**
    - Core 모듈은 상위 개념이므로 하위(구체적) 구현 내용을 알면 안 됩니다.

