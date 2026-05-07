# [LogExRunnerPlay] 로그 카테고리 중복 정의 (C2011)

## 이슈 설명
- **현상**: `ExRunnerPlayRuntime` 모듈 컴파일 중 `error C2011: 'FLogCategoryLogExRunnerPlay': 'struct' 형식 재정의` 에러 발생.
- **원인**: 여러 `.cpp` 파일에서 동일한 이름의 로그 카테고리를 `DEFINE_LOG_CATEGORY_STATIC`으로 정의함. 유니티 빌드 시 이 파일들이 병합되면서 구조체 중복 정의 발생.

## 해결 방법
- 로그 카테고리를 모듈 헤더(`ExRunnerPlayRuntimeModule.h`)에서 `DECLARE_LOG_CATEGORY_EXTERN`으로 선언.
- 모듈 소스(`ExRunnerPlayRuntimeModule.cpp`)에서 `DEFINE_LOG_CATEGORY`로 정의.
- 충돌이 발생하던 소스 파일들에서 `DEFINE_LOG_CATEGORY_STATIC`을 제거하고 모듈 헤더를 포함하도록 수정.

## 관련 파일
- `ExRunnerPlayRuntimeModule.h` / `.cpp`
- `ExRunnerGameMode.cpp`
- `ExRunnerInputComponent.cpp`

## 키워드
- #LogCategory #C2011 #Redefinition #UnityBuild #ExRunnerPlay
