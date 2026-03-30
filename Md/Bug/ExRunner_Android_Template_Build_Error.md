# [버그 리포트] 안드로이드 빌드 시 ExActorUtil 템플릿 미인식 오류

## 이슈 요약
안드로이드(Clang) 및 유니티 빌드 환경에서 `ExRunnerMovementComponent.cpp` 컴파일 중 `UExActorUtil::FindComponentInHierarchy` 템플릿 함수를 인식하지 못하고 일반 값 비교 연산자로 오인하여 빌드가 실패하는 현상.

## 원인 분석
1. **Include 순서 문제**: `ExActorUtil.h`가 `ExRunnerInputComponent.h`보다 먼저 include되어, 템플릿 인스턴스화 시점에 `UExRunnerInputComponent`가 미완전 타입(Incomplete Type) 상태였음.
2. **템플릿 모호성**: `UActorComponent*`를 인자로 받는 템플릿 오버로드가 존재함에도 불구하고, Clang 컴파일러가 특정 상황에서 `<` 기호를 템플릿 시작이 아닌 비교 연산자로 잘못 파싱함.

## 해결 방법
1. **Include 순서 조정**: `ExRunnerInputComponent.h`를 `ExActorUtil.h`보다 상단으로 배치하여 완전한 타입 정의를 먼저 파싱하도록 보장.
2. **호출 명시화**: `FindComponentInHierarchy<T>(this)` 대신 `FindComponentInHierarchy<T>(GetOwner())`를 호출하여 `AActor*` 버전의 템플릿을 직접 타겟팅함으로써 컴파일러의 과부하 및 모호성 제거.

## 결과
- 안드로이드 환경에서의 템플릿 파싱 에러 해결 및 빌드 정상화 확인.
