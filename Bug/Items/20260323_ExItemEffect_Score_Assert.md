# 버그 리포트: ExItemEffect_Score PlayerState 어서트 발생

**날짜:** 2026-03-23
**모듈:** ExCoreRuntime
**관련 시스템:** 아이템 시스템 (점수 획득 이펙트)
**키워드:** `ensureAlwaysMsgf`, `PlayerState`, `ExItemEffect_Score`, `Assert`

## 증상 (Issue)
에디터 플레이(PIE) 상태에서 플레이어 캐릭터가 코인(아이템)과 충돌(오버랩) 시 아래와 같은 로그와 함께 엔진 실행이 일시 중지(Assert/Break)되는 현상 발생:
```text
[ExItemEffect_Score] PlayerState가 ExPlayerStateBase가 아닙니다.
```

## 원인 분석 (Root Cause)
1. `UExItemEffect_Score::Execute_Implementation` 내부에서 `Instigator` 및 `PlayerState`의 유효성을 검사할 때, 언리얼 엔진의 강력한 단언문인 `ensureAlwaysMsgf` 매크로를 사용했습니다.
2. 런타임 게임 플레이 중 각종 오버랩 이벤트는 시점(PlayerState가 미처 다 초기화되기 전이거나)이나 대상(오버랩을 발동시킨 주체가 AI 폰이거나 콜리전 설정 오류로 인한 અન્ય 폰일 경우)에 따라 유효하지 않은 `PlayerState`를 반환할 수 있습니다.
3. 이때 예외 처리로 부드럽게 무시하고 넘어가야 할 상황을 에디터 Break 로직(`ensureAlwaysMsgf`)으로 처리하여 정상적인 테스트 흐름을 방해했습니다.
4. 더불어 `PC->PlayerState`를 직접 참조하는 것보다, 언리얼 5 표준인 `Pawn->GetPlayerState<T>()` 인터페이스를 사용하는 것이 더 안전합니다.

## 해결 방법 (Resolution)
1. `ExItemEffect_Score.cpp` 파일 내에 있던 공격적인 `ensureAlwaysMsgf` 검사 로직을 모두 일반적인 `if (!Pointer)` 분기문으로 교체했습니다.
2. `Instigator`나 `Pawn` 캐스팅이 실패할 경우 조용히 반환하도록 처리했습니다.
3. 점수를 추가하기 위한 PlayerState 참조를 `Pawn->GetPlayerState<AExPlayerStateBase>()`로 변경하고, 유효하지 않을 경우 `Warning` 로그만 띄우고 종료하도록 수정했습니다.

## 결과 및 후속 조치
- 더 이상 일반적인 오버랩 상황에서 에디터가 멈추지 않으며, 해당 컴포넌트를 가진 폰만이 정상적으로 점수를 획득할 수 있도록 안정성이 확보되었습니다.
- 아이템 콜리전 및 다른 이펙트류에서도 런타임 오버랩 시 `ensure` 사용을 지양하고 일반 분기로 무효화 처리를 권장합니다.
