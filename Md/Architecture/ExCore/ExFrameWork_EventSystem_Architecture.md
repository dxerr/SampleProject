# ExCore GameplayTag 이벤트 시스템 가이드

ExCore 모듈에 구현된 GameplayTag 기반 이벤트 발행/구독 시스템의 사용법과 구조를 설명합니다.

---

## 1. 개요

### 목적
- **디커플링**: GameMode, MovementComponent, BP 등 시스템 간 직접 참조 없이 이벤트 통신
- **확장성**: 새로운 리스너를 기존 코드 수정 없이 추가 가능
- **타입 안전성**: C++ Native Tag로 컴파일 타임 검증

### 설계 패턴
Lyra 프로젝트의 `UGameplayMessageRouter` 패턴을 참고하여 `UWorldSubsystem` 기반으로 구현.

---

## 2. 파일 구조

```
ExCore/Source/ExCoreRuntime/
├── Events/
│   ├── ExGameplayEventSubsystem.h      // 핵심 서브시스템
│   └── ExGameplayEventSubsystem.cpp
├── Tags/
│   ├── ExGameplayTags.h                // Native Tag 선언
│   └── ExGameplayTags.cpp              // Native Tag 정의
└── Util/Events/
    ├── ExGameplayEventLibrary.h        // BP 헬퍼 함수
    └── ExGameplayEventLibrary.cpp
```

---

## 3. 정의된 GameplayTags

| 태그 | 설명 | 용도 |
|------|------|------|
| `Ex.Action.Climb.Start` | 클라이밍 시작 | 트레드밀 정지 |
| `Ex.Action.Climb.End` | 클라이밍 종료 | 트레드밀 재개 |
| `Ex.Action.Vault.Start` | 볼팅 시작 | 확장용 |
| `Ex.Action.Vault.End` | 볼팅 종료 | 확장용 |
| `Ex.Action.Slide.Start` | 슬라이딩 시작 | 확장용 |
| `Ex.Action.Slide.End` | 슬라이딩 종료 | 확장용 |

---

## 4. 사용법

### 4.1 C++에서 이벤트 발행

```cpp
#include "ExGameplayTags.h"
#include "ExGameplayEventSubsystem.h"

void UMyComponent::TriggerClimbStart()
{
    if (UWorld* World = GetWorld())
    {
        if (auto* EventSub = World->GetSubsystem<UExGameplayEventSubsystem>())
        {
            // 간단 버전 (Instigator만 전달)
            EventSub->BroadcastEventSimple(TAG_Ex_Action_Climb_Start, this);
            
            // 상세 버전 (Payload 포함)
            FExGameplayEventPayload Payload;
            Payload.Instigator = this;
            Payload.Target = SomeActor;
            Payload.OptionalValue = 1.5f;
            EventSub->BroadcastEvent(TAG_Ex_Action_Climb_Start, Payload);
        }
    }
}
```

### 4.2 C++에서 이벤트 수신

```cpp
#include "ExGameplayTags.h"
#include "ExGameplayEventSubsystem.h"

void AMyGameMode::BeginPlay()
{
    Super::BeginPlay();
    
    if (auto* EventSub = GetWorld()->GetSubsystem<UExGameplayEventSubsystem>())
    {
        // 특정 태그에 리스너 등록
        EventSub->GetEventDelegate(TAG_Ex_Action_Climb_Start)
            .AddDynamic(this, &AMyGameMode::OnClimbStart);
    }
}

void AMyGameMode::OnClimbStart(FGameplayTag EventTag, const FExGameplayEventPayload& Payload)
{
    // 이벤트 처리
    UE_LOG(LogExFrameWork, Log, TEXT("Climb Started by: %s"), 
        Payload.Instigator ? *Payload.Instigator->GetName() : TEXT("Unknown"));
}
```

### 4.3 Blueprint에서 이벤트 발행

1. **Broadcast Gameplay Event** 노드 검색
2. `Event Tag` 핀에 `Ex.Action.Climb.Start` 입력
3. `Instigator` 핀에 Self 또는 발행 주체 연결

![BP 발행](./Images/BP_BroadcastEvent.png) *(이미지 미첨부)*

### 4.4 Blueprint에서 이벤트 수신

1. **Get Ex Gameplay Event Subsystem** 노드로 서브시스템 획득
2. `On Gameplay Event` 델리게이트 바인딩
3. 이벤트 수신 시 `Event Tag`로 필터링 처리

```
[Get Subsystem] → [Bind Event to On Gameplay Event]
                         ↓
              [Event: Compare Tag] → [처리 로직]
```

---

## 5. 핵심 클래스 API

### UExGameplayEventSubsystem

| 함수 | 설명 |
|------|------|
| `BroadcastEvent(Tag, Payload)` | 상세 데이터와 함께 이벤트 발행 |
| `BroadcastEventSimple(Tag, Instigator)` | Instigator만 포함하여 간단 발행 |
| `GetEventDelegate(Tag)` | C++ 태그별 델리게이트 반환 (AddDynamic용) |
| `HasListeners(Tag)` | 해당 태그에 리스너가 있는지 확인 |

### UExGameplayEventLibrary (BP 헬퍼)

| 함수 | 설명 |
|------|------|
| `BroadcastGameplayEvent(Context, Tag, Instigator)` | BP에서 간편 이벤트 발행 |
| `GetExGameplayEventSubsystem(Context)` | BP에서 서브시스템 획득 |

---

## 6. 확장 가이드

### 새 태그 추가

1. `ExGameplayTags.h`에 선언 추가:
```cpp
EXCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ex_Action_NewAction_Start);
```

2. `ExGameplayTags.cpp`에 정의 추가:
```cpp
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ex_Action_NewAction_Start, 
    "Ex.Action.NewAction.Start", "새 액션 시작 설명");
```

3. (선택) `Config/DefaultGameplayTags.ini`에도 등록

### 새 리스너 추가

기존 코드 수정 없이 BeginPlay에서 델리게이트 등록만 추가:
```cpp
EventSub->GetEventDelegate(TAG_Ex_Action_NewAction_Start)
    .AddDynamic(this, &UMyNewComponent::OnNewActionStart);
```

---

## 7. 참고사항

- **WorldSubsystem 생명주기**: 월드 생성 시 자동 인스턴스화, 월드 파괴 시 자동 정리
- **네트워킹**: 현재 로컬 전용 (RPC 복제 필요 시 별도 구현)
- **의존성**: `GameplayTags` 모듈 필요 (Build.cs에 추가됨)

---

*최종 수정: 2026-02-09*
