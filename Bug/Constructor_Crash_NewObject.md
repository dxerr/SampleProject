# 🐛 Bug Report: Constructor Crash due to SetBoxExtent

## 🚨 Issue
**Fatal error**: `NewObject with empty name can't be used to create default subobjects` inside a Constructor.

### Error Log
```
Fatal error: [File:UObjectGlobals.cpp] [Line: 4998] 
NewObject with empty name can't be used to create default subobjects...

CallStack:
UnrealEditor-Engine.dll!UBoxComponent::UpdateBodySetup()
UnrealEditor-Engine.dll!UBoxComponent::SetBoxExtent()
UnrealEditor-ExCoreRuntime.dll!UExObstacleInteractionComponent::UExObstacleInteractionComponent()
```

## 🕵️‍♂️ Root Cause
Call `SetBoxExtent()` inside the **Constructor** (`UExObstacleInteractionComponent::UExObstacleInteractionComponent`).
- `SetBoxExtent` attempts to update the physics state (`UpdateBodySetup`).
- During construction, the component is not fully initialized, causing `NewObject` (for `BodySetup`) to fail or be called with invalid parameters/empty name context.

## ✅ Solution
Use **`InitBoxExtent()`** instead of `SetBoxExtent()` inside the Constructor.
- `InitBoxExtent` only sets the variable `BoxExtent` without triggering physics update events.

```cpp
// ❌ BAD (Crash in Constructor)
SetBoxExtent(FVector(50.f));

// ✅ GOOD (Safe for Constructor)
InitBoxExtent(FVector(50.f));
```

## 📚 Lesson
- **Constructors** should only initialize data.
- Avoid functions that trigger **Unreal Engine Subsystems** (Physics, Rendering updates) inside the constructor.
- Look for `Init...` prefix methods for components (e.g., `InitSphereRadius`, `InitCapsuleSize`).
