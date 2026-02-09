# ExProject Workspace 규칙

이 프로젝트 작업 시 반드시 다음 규칙을 준수하고 숙지해야 합니다.

## 1. 코딩 및 설계 규칙
1. **명명 규칙 (Naming)**:
   - 클래스/구조체 접두사 **"Ex"** 필수 (`UEx`, `AEx`, `FEx`, `EEx`, `IEx`)
   - 에셋 명명: `BP_Ex[Name]`, `WBP_Ex[Name]`, `DA_Ex[Name]`, `DT_Ex[Name]`
   - Boolean 변수: `b` 접두사 필수 (예: `bIsReady`)
2. **접근 지정자 (Access)**: `public`/`private` 구분을 엄격히 하지 않으며, 접근성을 우선합니다 (특수 상황 제외).
3. **생성자 (Constructor)**: 
   - `NewObject` 금지 → `CreateDefaultSubobject` 사용
   - 물리/렌더링 갱신 함수 호출 금지 (`Init...` 계열 함수 사용)
4. **UE5 베스트 프랙티스**:
   - `TObjectPtr<T>` 사용 권장
   - 로그 카테고리: `LogExFrameWork` 사용
   - 검증: `check()` (치명적), `ensure()` (경고)
5. **데이터 드리븐**: 하드코딩 배제, `UPROPERTY(EditAnywhere)` 등으로 에디터 노출
6. **헤더 파일**: `.generated.h`는 **반드시** 마지막 `#include`로 위치

## 2. 폴더 및 모듈 구조
1. **폴더 구조**:
   - `Struct/`: 구조체 정의 (Hierarchy 따름)
   - `Data/`: 데이터 에셋 및 관련 구조체
   - `Util/`: 유틸리티성 함수 (Static, Math, Find 등)
2. **모듈 분리 원칙**:
   - **Core (ExCore)**: 범용 프레임워크 (장르 무관)
   - **Feature (ExRunnerPlay 등)**: 특정 장르/기능 특화
   - **의존성**: Feature -> Core (O), Core -> Feature (X) 절대 금지

## 3. 네트워킹 및 주석
1. **네트워킹**: 모든 로직은 **서버 권한(HasAuthority)** 고려 필수 (Dedicated Server 기준)
2. **주석**:
   - 클래스/함수 상단에 역할 설명 (한글/영문 혼용 가능)
   - 파일 상단 헤더 주석 필수
   - `TODO`, `FIXME`로 작업 필요 지점 명시
