# [Build] Mobile AsyncLoading & Dependency Crash 통합 리포트

## 1. 개요
안드로이드 Shipping 빌드에서 비동기 로딩 중 발생하는 `ExInputComponentBase` 관련 크래시와 GameFeature 데이터셋 참조 오류를 해결하여 앱의 실행 안정성을 확보했습니다.

## 2. 주요 문제 및 해결 내용

### A. 추상 클래스 CDO 직렬화 크래시
*   **증상**: `UExInputComponentBase`를 비동기 로딩할 때 간헐적으로 Fatal Error 발생.
*   **원인**: 추상 클래스임에도 불구하고 `Blueprintable` 및 `BlueprintType` 플래그가 설정되어 있어, 에디터가 비정상적인 CDO(Class Default Object)를 생성 및 직렬화 시도.
*   **해결**: 기본 클래스에서 블루프린트 생성 플래그를 제거하고 자식 클래스인 `UExRunnerInputComponent`에서만 허용하도록 변경.

### B. GameFeature 하드 레퍼런스 오류 (`ErrorWaitingForDependencies`)
*   **증상**: 특정 플러그인 활성 시 `ErrorWaitingForDependencies` 상태로 멈춤.
*   **원인**: `GameFeatureData` 에셋이 `Shipping` 빌드에서 패키징 누락되거나, 활성 상태가 `Inactive`로 잘못 설정되어 상호 간의 하드 레퍼런스가 깨짐.
*   **해결**:
    *   `BuiltInInitialFeatureState`를 `Active`로 강제 설정.
    *   패키징 세팅에 `GameFeatureData`를 명시적으로 `Additional Asset Directory`로 추가.

## 3. 핵심 수정 파일
*   `UExInputComponentBase.h` (클래스 플래그 수정)
*   `ExRunnerPlay.uplugin`
*   `DefaultGame.ini` (Cooker 설정)

## 4. 최종 결과
이제 모바일 기기에서도 비동기 로딩 중 크래시 없이 안정적으로 게임이 시작되며, 모든 확장 기능(GameFeature)들이 정상적으로 로드됩니다.
