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

## 2. 문서 관리
- 이 파일(`ExFrameWork_Guidelines.md`)은 프로젝트의 살아있는 규칙 문서로 관리됩니다.
- 새로운 규칙이나 변경 사항이 발생하면 즉시 이 문서를 업데이트해야 합니다.
