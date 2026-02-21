# Phase 3: 시각화 계층 — DebugDraw + Gameplay Debugger

> 최종 갱신: 2026-02-21

---

## 1. UExDebugDrawSubsystem (TickableWorldSubsystem)

`ExCore/Debug/ExDebugDrawSubsystem.h/.cpp`

### 핵심 컨셉
- **카테고리 조건부** — `DebugStateSubsystem.IsCheatEnabled(Tag)` 체크 후 그리기
- **Persistent Draw** — 매 프레임 반복 호출이 필요한 시각화를 등록/해제로 관리
- **Shipping 빌드 제외** — `ShouldCreateSubsystem`에서 자동 비활성화

### API 목록

| 메서드 | 기능 |
|--------|------|
| `DrawLineChecked(Tag, Start, End, Color, Thickness, Duration)` | 조건부 라인 |
| `DrawSphereChecked(Tag, Center, Radius, Segments, Color, Duration)` | 조건부 구체 |
| `DrawBoxChecked(Tag, Center, Extent, Color, Duration)` | 조건부 박스 |
| `DrawScreenTextChecked(Tag, Text, Color, Duration)` | 조건부 화면 텍스트 |
| `RegisterPersistentDraw(Tag, DrawFunc) → int32` | 매 프레임 그리기 등록 |
| `UnregisterPersistentDraw(Id)` | 등록 해제 |
| `ClearPersistentDraws(Tag)` | 카테고리 전체 해제 |

### 사용 예시 (ExRunnerPlay)
```cpp
auto* DD = GetWorld()->GetSubsystem<UExDebugDrawSubsystem>();

// 일회성
DD->DrawLineChecked(TAG_Ex_Debug_Path, Start, End, FColor::Green, 2.f, 5.f);

// 매 프레임 (Persistent)
int32 Id = DD->RegisterPersistentDraw(TAG_Ex_Debug_Path, 
    [this](UWorld* W, float DT) {
        DrawDebugLine(W, SegStart, SegEnd, FColor::Cyan, false, 0.f, 0, 3.f);
    });

// 해제
DD->UnregisterPersistentDraw(Id);
```

## 2. FExRunnerDebuggerCategory (Gameplay Debugger)

`ExRunnerPlay/Debug/ExRunnerDebuggerCategory.h/.cpp`

### 표시 섹션

| 섹션 | 데이터 | 색상 |
|------|--------|------|
| **[Path]** | SegmentCount, TotalDistance, CurrentAlpha | Cyan |
| **[Chunk]** | ActiveChunks, PoolSize, TotalSpawned | Cyan |
| **[Slope]** | HeightOffset, SlopePitchAngle, ConsecutiveTurnCount | Green/Red |
| **[Speed]** | CurrentSpeed, TargetSpeed | Cyan |

### 등록 방법
모듈 `StartupModule()`에서 등록:
```cpp
#if WITH_GAMEPLAY_DEBUGGER
IGameplayDebugger& Debugger = IGameplayDebugger::Get();
Debugger.RegisterCategory("ExRunner",
    IGameplayDebugger::FOnGetCategory::CreateStatic(
        &FExRunnerDebuggerCategory::MakeInstance),
    EGameplayDebuggerCategoryState::EnabledInGame);
#endif
```

### 사용법
PIE 실행 중 **`'`** (Apostrophe) 키 → 카테고리 목록에서 "ExRunner" 선택

> [!NOTE]
> `CollectData` 내부의 PathManager/ChunkSpawner 참조는 향후 해당 컴포넌트에
> Getter 함수 추가 후 연결 예정 (TODO 마커 포함)
