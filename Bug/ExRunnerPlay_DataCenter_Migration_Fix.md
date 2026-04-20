# [Bug] ExRunnerPlay DataCenter 마이그레이션 후 스폰 장애 및 경계 이탈

## 1. 이슈 개요
- **증상 1**: `ExRunnerPlay` 데이터 에셋을 `ExDataCenter`로 통합한 후, `RunnerConfig`가 `None`으로 조회되어 경로가 직선으로만 생성됨.
- **증상 2**: `RunnerConfig` 주입 성공 후에도 청크가 월드 경계(스카이박스)를 뚫고 나가는 현상 발생.

## 2. 원인 분석
### 증상 1 (데이터 미주입)
- **클래스 인덱싱 문제**: 블루프린트로 생성된 데이터 에셋(BP_DA)의 클래스가 `UExRunnerConfig`가 아닌 동적 생성 클래스(`BP_..._C`)였음. 기존 `ExDataCenterSubsystem`은 리프 클래스(Leaf Class)로만 인덱싱을 수행하여 네이티브 기반(`GetConfig<UExRunnerConfig>`) 조회 시 데이터를 찾지 못함.
- **타이밍 문제**: `GameMode::BeginPlay` 시점이 `GameFeatureAction`의 데이터 등록 완료 시점보다 빨랐음.

### 증상 2 (경계 이탈)
- **에셋 설정 불일치**: 기존 `DA_ExCurveConfig`에 설정되어 있던 월드 경계값(예: +/- 5,000)이 새로운 통합 에셋(`DA_ExRunnerConfig`)을 생성하는 과정에서 기본값(+/- 50,000)으로 유지됨.

## 3. 해결 과정
### 시스템 수정
- **클래스 계층 인덱싱 지원**: `ExDataCenterSubsystem::RegisterConfig` 및 `RegisterDefinition` 함수를 수정하여 에셋의 클래스뿐만 아니라 모든 부모 클래스 계층(`UExConfigDataAsset`까지)을 키로 등록하도록 개선.
- **초기화 타이밍 보정**: `ExRunnerGameMode::StartRunnerGame` 시점에서 `RunnerConfig`가 유효하지 않을 경우 재조회(Fallback)를 수행하는 로직 추가.

### 데이터 수정
- **에셋 값 대조**: 디버그 로그를 통해 현재 적용 중인 `WorldBounds` 값을 실시간 확인하고, 이를 맵 크기(스카이박스 범위)에 맞게 에셋 수치를 조정.

## 4. 최종 결과
- `RunnerConfig`가 정상적으로 주입되어 곡선 및 장애물 스폰 로직이 복구됨.
- 월드 경계값을 맵 크기에 맞게 설정함으로써 스카이박스 이탈 버그 해결.
- 불필요해진 레거시 헤더 파일 및 에셋 삭제 완료.
