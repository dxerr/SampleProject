# ExRunnerGameMode 조향 및 거리 추적 로직 리팩터링 및 버그 수정

## 관련 이슈
- 클라이언트-서버 동기화(클라이언트측 시각적 회전 예측 보간 지원)를 위한 조향(Steering) 로직 분리 필요
- GameMode에 종속되어 있던 PathManager로 인해, 클라이언트(ChunkSpawner 등)가 GetGameState<...>() 접근 시 PathManager 정보 획득에 실패함

## 원인 분석
1. **아키텍처 문제**: 클라이언트에서는 GameMode의 인스턴스가 존재하지 않기 때문에, 서버 전용 공간인 GameMode에 `PathManager`나 달린 거리 변수(`CurrentPathDistance`)가 있으면 클라이언트에서는 참조가 불발됩니다.
2. **BP GameState 미지정 에러**: 코드상으로는 `AExRunnerGameState`를 새로 파서 PathManager를 이관했으나, 에디터 내 구형 `BP_ExRunnerGameMode`에 할당된 Game State Class가 이전 값 그대로 남아 있어, `GetGameState<AExRunnerGameState>()` 캐스팅 실패. 결과적으로 ChunkSpawner가 곡선이 아닌 레거시 직선 1000 단위로만 반복 스폰하는 버그(Fallback)가 발생.
3. **코드 이관 시 의존성 누락**: 복잡한 헤더 포함관계(`ExGameplayTags.h`, `Character.h`, 디버그 드로우 함수 등)가 흩어져 있어 컴파일 타임 에러(C2065 등) 및 디버깅 가시성 누락 유발.

## 해결 과정
1. **변수 및 매니저의 GameState 이관**
   - 기존 `ExRunnerGameMode`에 있던 `PathManager` 포인터와 `CurrentPathDistance`, `RealPlayerPathDistance` 멤버 변수를 `ExRunnerGameState`로 이전하여 클라이언트/서버 간 리플리케이션 및 Public 접근 가능 상태로 변경.
   - `ExChunkSpawner`, `ExObstacleManager`, `ExFloorChunk` 등 기존에 의존성을 가지던 서브 컴포넌트들을 모두 GameState 기반 획득으로 일일이 수정.
2. **조향 보간 로직 Movement Component 로 편입**
   - 틱 단위로 캐릭터 회전 오차를 P-Control 기반 보정/개입하던 기존 로직(`UpdateCharacterRotation`) 전체를 `UExRunnerMovementComponent` 내부로 이전.
   - 플레이어 Velocity 기반 LookAhead 산출, Controller Rotator 업데이트, 드리프트 보정 물리 이동(AddActorWorldOffset) 등 구현.
   - 누락되었던 **디버그 드로잉 코드(X,Y,Z 좌표축 드로우, 오차 노란선, 화면 출력 디버그)** 모두 복구.
3. **BP GameState 기본 클래스 안전장치 배포**
   - `AExRunnerGameMode` 생성자에 `GameStateClass = AExRunnerGameState::StaticClass();` 를 명시하여 빈 프로젝트나 초기 설정 시 에러가 나지 않도록 고정.
4. **결과 검증 및 Git 배포**
   - 로컬 테스트를 통해 곡선 청크 스폰 회복 확인, 스폰 오류 로그가 사라짐을 확인, Movement Component 를 통한 정상적인 캐릭터 회전 및 원심력 보정 시각화 검증 완료.

## 후속 논의(선택 사항)
- 아직 네트워크 리플리케이션을 통한 다수 클라이언트 폰의 부드러운 위치/회전 보정 검증 등 본격적인 멀티플레이 대역 점검은 추가 테스트가 필요할 수 있습니다.
