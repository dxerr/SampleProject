# Bug: 코인 획득 이벤트 UI 미갱신 문제 (PlayerState 종속 허점)

## 📌 Issue Summary
**현상:** ExRunnerPlay 맵에서 코인 아이템 획득 시 폰의 `Score` 스탯은 정상 작동하는 듯 보이나 (혹은 Null 오류), 정작 UI `ExRunnerStatsViewModel`에 바인딩된 코인 수치 텍스트가 증가하지 않음.
**키워드:** `Issue`, `ItemEffect_Score`, `BroadcastEventSimple`, `Payload`, `PlayerState`, `MVVM`

---

## 🔍 Troubleshooting Process

### 1. 로그 분석 및 증상 파악
- 대시 버프(`ExItemEffect_Buff`) 활성화 시 UI 스프린트 타이머는 실시간으로 잘 업데이트 됨.
- 반면 코인 효과(`ExItemEffect_Score`) 아이템을 획득해도, UI 쪽 컴포넌트(`UExRunnerStatComponent::OnScorePickedUp`)로 코인 획득 이벤트가 도달해도 값이 `0`으로 처리되는지, 아예 도달하지 않는지 의심됨.

### 2. Payload 발송 원인 (1차 수정)
- 원인 분석 1: `ExItemEffect_Score` 소스에서 UI로 알리는 이벤트 브로드캐스트를 `BroadcastEventSimple`로 보내고 있어 `OptionalValue = 0` 빈 껍데기가 날아가는 것을 확인.
- 조치: `FExGameplayEventPayload`를 생성해 `Payload.OptionalValue = ScoreAmount`를 명시하고 `BroadcastEvent` 호출로 변경.

### 3. 브로드캐스트 미실행 구조 허점 (2차, 근본 원인 파악)
- Payload를 채워주었음에도 UI가 반응하지 않음.
- 원인 분석 2: 함수 첫 줄 부근 로직 점검 결과, 아이템을 얻은 액터(Instigator)에서 `GetPlayerState<AExPlayerStateBase>()`를 수행할 때 Null일 경우(Runner 단순 캐릭터 폰 특성상 PlayerState가 없거나 Replication Delay인 경우) **이른 반환(`return;`)**을 시켜버림.
- 결론: PlayerState가 Null이라서, 그 아래에 위치한 "이벤트 이벤트 브로드캐스트 로직" 자체가 실행되지 못하고 abort 되고 있었음.

---

## ✅ Resolution

`ExItemEffect_Score.cpp` (경로: `Plugins/GameFeatures/ExCore/Source/ExCoreRuntime/Items/Effects/`) 수정:

**[Before]**
```cpp
AExPlayerStateBase* PS = Pawn->GetPlayerState<AExPlayerStateBase>();
if (!PS)
{
    // 경고 출력 후 무조건 종료되어 UI 방송도 스킵됨
    return;
}
PS->AddScore(ScoreAmount);

// (하단 브로드캐스트 로직 도달 실패)
```

**[After]**
```cpp
AExPlayerStateBase* PS = Pawn->GetPlayerState<AExPlayerStateBase>();
if (PS)
{
    PS->AddScore(ScoreAmount);
}
else
{
    // PlayerState가 없어도 경고만 띄우고 아래의 UI 연동 이벤트 브로드캐스트 로직은 정상적으로 타도록 허용
    UE_LOG(..., "의 PlayerState를 찾지 못했습니다. (UI 연동용 이벤트는 계속 발생합니다)");
}

// Payload 생성 및 ScoreAmount 적재
FExGameplayEventPayload Payload;
Payload.Instigator = Instigator;
Payload.Target = Pawn;
Payload.OptionalValue = ScoreAmount; // 코인 증가량

// 정상적으로 UI에게 Broadcast
EventSub->BroadcastEvent(TAG_Ex_Item_PickedUp_Score, Payload);
```

**결과:** RunnerPlay 등 PlayerState 시스템을 엄격히 쓰지 않는 스테이트리스 기반 아케이드 모드에서도 아이템 효과로 인한 스탯 연동 이벤트가 무조건 통과되도록 보장하여 MVVM UI와 완벽하게 동기화됨.
