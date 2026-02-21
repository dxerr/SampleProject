# Phase 2: CheatManagerExtension — 모듈별 콘솔 명령

> 최종 갱신: 2026-02-21

---

## 1. 설계 원칙

| 원칙 | 구현 |
|------|------|
| Feature → Core 단방향 의존 | ExRunnerPlay는 ExCoreRuntime만 참조, 역방향 없음 |
| 자동 등록 | `UGameFeatureAction_AddCheats`로 Feature 활성화 시 Extension 자동 연결 |
| 관심사 분리 | Core 범용 치트 vs Feature 전용 치트 분리 |

## 2. ExCoreCheatExtension (범용)

`ExCore/Debug/ExCoreCheatExtension.h/.cpp`

| 콘솔 명령 | 기능 | 내부 동작 |
|-----------|------|----------|
| `ExGodMode` | 무적 모드 토글 | TAG_Ex_Debug_GodMode + Pawn.SetCanBeDamaged |
| `ExSetSpeed [float]` | 이동속도 변경 | CMC.MaxWalkSpeed 직접 수정 (0=복원) |
| `ExSlowMo [float]` | 글로벌 타임 | SetGlobalTimeDilation (0.01~10.0) |
| `ExShowDebugAll` | 전체 디버그 토글 | 모든 TAG_Ex_Debug_* 일괄 Set |
| `ExSetDebugValue [Cat] [Val]` | 카테고리 수치 | DebugStateSubsystem.SetCheatValue |

## 3. ExRunnerCheatExtension (Runner 전용)

`ExRunnerPlay/Debug/ExRunnerCheatExtension.h/.cpp`

| 콘솔 명령 | 기능 | 연동 Tag |
|-----------|------|----------|
| `ExRunnerShowPath` | 경로 시각화 | TAG_Ex_Debug_Path |
| `ExRunnerShowChunk` | 청크 시각화 | TAG_Ex_Debug_Chunk |
| `ExRunnerForceSlope` | 꽈배기 강제 발동 | TAG_Ex_Debug_Slope |
| `ExRunnerSetSlopeTrigger [N]` | SlopeTriggerCount 오버라이드 | TAG_Ex_Debug_Slope (Value) |
| `ExRunnerShowHeightOffset` | HeightOffset 로그 | TAG_Ex_Debug_Slope |
| `ExRunnerShowSpeed` | 속도 디버그 | TAG_Ex_Debug_Speed |

## 4. GameFeature 등록 방법

> [!IMPORTANT]
> **에디터에서 설정 필요**: 각 Feature의 GameFeatureData 에셋에 AddCheats Action 추가

```
ExCore.uplugin → GameFeatureData:
  Actions → UGameFeatureAction_AddCheats
    CheatManagers → [UExCoreCheatExtension]

ExRunnerPlay.uplugin → GameFeatureData:
  Actions → UGameFeatureAction_AddCheats
    CheatManagers → [UExRunnerCheatExtension]
```

## 5. 데이터 흐름

```
콘솔: "ExRunnerForceSlope"
  → UExRunnerCheatExtension::ExRunnerForceSlope()
    → UExDebugStateSubsystem::ToggleCheat(TAG_Ex_Debug_Slope)
      → CheatStates[TAG_Ex_Debug_Slope].bEnabled = true
      → BroadcastStateChange()
        → UExGameplayEventSubsystem::BroadcastEvent(TAG_Ex_Debug_Slope, Payload)
          → PathManager (구독) → bDebugSlopeForced = true
```
