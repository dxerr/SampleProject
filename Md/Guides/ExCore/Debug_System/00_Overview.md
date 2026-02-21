# ExCore 디버그/치트 시스템 — 전체 개요

> 최종 갱신: 2026-02-21 (Phase 1 구현 완료)

---

## 1. 시스템 목적

개발 단계에서 Feature 모듈(ExRunnerPlay 등)의 기능을 **쉽게 디버깅/검증**할 수 있는 범용 인프라를 ExCore에 구축합니다.

## 2. 아키텍처 (Plan F — 하이브리드 통합형)

```
┌─ ExCore (디버그 인프라) ─────────────────────────────┐
│                                                       │
│  UExCheatManager ←── PlayerController.CheatClass      │
│    └─ UFUNCTION(Exec): ExDebug, ExDebugStatus 등      │
│                                                       │
│  UExDebugStateSubsystem (GameInstanceSubsystem)       │
│    └─ Tag별 상태 Map (레벨 간 유지)                    │
│    └─ 상태 변경 시 → ExGameplayEventSubsystem 브로드캐스트│
│                                                       │
│  ExGameplayTags.h — TAG_Ex_Debug_Path, Chunk, Slope...│
│  ExGameplayEventSubsystem — 기존 Pub/Sub 재활용        │
└───────────────────────────────────────────────────────┘
         ↓ Feature는 Core만 참조
┌─ ExRunnerPlay (Feature 디버그) ──────────────────────┐
│  UExRunnerCheatExtension  (Phase 2)                   │
│  FExRunnerDebuggerCategory (Phase 3)                  │
│  DA_ExRunnerCheats DataAssets (Phase 4)                │
└───────────────────────────────────────────────────────┘
```

## 3. 구현 단계

| Phase | 내용 | 상태 |
|-------|------|:----:|
| **Phase 1** | CheatManager + DebugStateSubsystem + 디버그 태그 | ✅ 완료 |
| **Phase 2** | CheatManagerExtension (Core/Runner) | ✅ 완료 |
| **Phase 3** | DebugDrawSubsystem + GameplayDebugger Category | ✅ 완료 |
| **Phase 4** | DataAsset + 동적 UI | 📋 예정 |

## 4. 파일 맵

```
ExCore/Source/ExCoreRuntime/
├── Debug/
│   ├── ExDebugTypes.h              ← 열거형/구조체
│   ├── ExCheatManager.h/.cpp       ← Base CheatManager
│   ├── ExDebugStateSubsystem.h/.cpp← 전역 디버그 상태 (GI Subsystem)
│   ├── ExCoreCheatExtension.h/.cpp ← Core 범용 치트 Extension
│   └── ExDebugDrawSubsystem.h/.cpp ← 카테고리별 시각화 (World Subsystem)
├── Tags/
│   └── ExGameplayTags.h/.cpp       ← TAG_Ex_Debug_* 태그 정의
└── Events/
    └── ExGameplayEventSubsystem.*  ← 기존 Pub/Sub (재활용)

ExRunnerPlay/Source/ExRunnerPlayRuntime/
└── Debug/
    ├── ExRunnerCheatExtension.h/.cpp   ← Runner 전용 치트 Extension
    └── ExRunnerDebuggerCategory.h/.cpp ← Gameplay Debugger 오버레이
```

## 5. 사용법

### 콘솔 명령 (PIE 환경)
```
Ex.Debug Path        → 경로 시각화 토글
Ex.Debug Slope       → 꽈배기 디버그 토글
ExDebugStatus        → 모든 상태 출력
ExDebugReset         → 상태 초기화
```

### C++ 코드에서 접근
```cpp
auto* DS = GetGameInstance()->GetSubsystem<UExDebugStateSubsystem>();
DS->SetCheatEnabled(TAG_Ex_Debug_Path, true);
if (DS->IsCheatEnabled(TAG_Ex_Debug_Slope)) { /* ... */ }
```

## 6. 관련 문서
- [01_DebugStateSubsystem.md](01_DebugStateSubsystem.md) — Phase 1 상세
- [02_CheatExtension.md](02_CheatExtension.md) — Phase 2 상세 (예정)
- [03_DebugDraw_Debugger.md](03_DebugDraw_Debugger.md) — Phase 3 상세 (예정)
- [04_DataDriven_UI.md](04_DataDriven_UI.md) — Phase 4 상세 (예정)
