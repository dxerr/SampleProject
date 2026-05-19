# Bug Report: MusicManager Dedicated Server Crash (ensure failure)

## 문제 설명 (Issue)
- **현상**: 데디케이티드 서버 환경에서 `UExMusicManagerSubsystem::StartBGM` 호출 시 `BaseLayerAudioComp`가 생성되지 않아 `ensure` 어설트가 발생함.
- **원인**: `ExRunnerGameMode`(서버 전용)에서 직접 오디오 재생을 시도했으나, 데디서버에는 오디오 디바이스가 없어 `SpawnSound2D`가 `nullptr`을 반환함.

## 해결 과정 (Resolution)
1. **방어 코드 추가**: `ExMusicManagerSubsystem::StartBGM` 내부에 `NM_DedicatedServer` 체크를 추가하여 서버 프로세스에서의 오디오 실행을 차단함.
2. **구조 개선**: BGM 재생 명령을 `GameMode`에서 `GameState`로 이관.
    - `AExRunnerGameState`에 `StageBGM` 변수를 추가하고 `ReplicatedUsing`으로 설정.
    - `OnRep_StageBGM`을 통해 클라이언트가 데이터를 복제받는 즉시 재생하도록 구현.
3. **통합 처리**: `SetStageBGM` 함수를 통해 Standalone/ListenServer/Dedicated 환경에 상관없이 일관된 방식으로 `MusicManager`를 호출하도록 통합함.

## 관련 키워드
`DedicatedServer`, `AudioCrash`, `ensure`, `GameState_Replication`, `MusicManager`

## 파일 변경 이력
- `ExMusicManagerSubsystem.cpp`: 데디서버 조기 리턴 추가.
- `ExRunnerGameState.h/cpp`: BGM 리플리케이션 및 재생 콜백 구현.
- `ExRunnerGameMode.cpp`: 호출부 이관.
