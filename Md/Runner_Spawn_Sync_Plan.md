# 🏃 트레드밀 시스템 V2: World Shift 동기화 계획

## 🎯 목표
주인님의 지침에 따라 **고정된 월드 좌표계(Static World Space)** 를 유지하는 **트레드밀 시스템**을 구축합니다. 이는 쉐이더(Curved World), 파티클, 조명 연출의 안정성을 위해 필수적입니다.

## 🚨 기존 문제 원인
1. **독립적인 이동**: 캐릭터와 바닥이 각자 다른 로직(`Mover` vs `Tick`)으로 움직여 속도 차이 발생.
2. **이중 이동(Double Movement)**: 서로 반대로 움직여 상대 속도가 2배가 됨.

## 🛠️ 해결 방안: World Shift (월드 시프트)

캐릭터가 **"물리적으로 이동한 만큼"** 세상(바닥)을 **"반대로 미는"** 방식입니다. 바닥은 스스로 움직이지 않고, 캐릭터의 이동량에 종속(Slave)됩니다.

### 1. 구조 변경 (Master-Slave)
- **Master (캐릭터)**: Mover 컴포넌트에 의해 전진합니다.
- **Slave (바닥)**: `AExFloorChunk`의 자체 이동 로직(`Tick`)을 **제거**합니다. 오직 캐릭터에 의해서만 위치가 바뀝니다.

### 2. 구현 로직 (매 프레임 처리)

`UExRunnerMovementComponent::TickComponent`의 마지막 단계에서 수행:

1.  **이동량 측정**: 이번 프레임에 캐릭터가 원점(0,0,0)에서 얼마나 벗어났는지(`DeltaX`) 측정합니다.
    ```cpp
    float CurrentX = TargetPawn->GetActorLocation().X;
    float DeltaX = CurrentX - FixedOriginX; // FixedOriginX = 0
    ```
2.  **월드 밀기 (World Shift)**: 활성화된 모든 바닥 청크(`ActiveChunks`)를 `-DeltaX` 만큼 이동시킵니다.
    ```cpp
    for (AActor* Chunk : Spawner->ActiveChunks)
    {
        Chunk->AddActorWorldOffset(FVector(-DeltaX, 0, 0));
    }
    ```
3.  **캐릭터 복귀 (Reset)**: 캐릭터를 다시 원점(`FixedOriginX`)으로 되돌립니다.
    ```cpp
    FVector ResetPos = TargetPawn->GetActorLocation();
    ResetPos.X = FixedOriginX;
    TargetPawn->SetActorLocation(ResetPos);
    ```

### 3. 스포너 로직 개선
- `KillZ` 판정은 이제 **캐릭터 위치(0)에 대한 상대 좌표**로 정확하게 동작합니다.
- 바닥이 뒤로 밀려나 `CurrentLocation.X < KillZ`가 되면 제거하고, 맨 앞에 새 청크를 붙입니다.
- **좌표 누적 문제 원천 봉쇄**: 캐릭터와 월드 중심이 항상 (0,0,0) 부근에 유지됩니다.

## 📅 작업 순서

1.  **`AExFloorChunk`**: `Tick` 함수 내 `AddActorWorldOffset` 코드 제거 (고정체로 변경).
2.  **`UExChunkSpawner`**: `ActiveChunks` 배열을 외부에 공개(Getter)하거나, 이동 처리를 돕는 함수(`ShiftChunks`) 구현.
3.  **`UExRunnerMovementComponent`**: `TickComponent`에 **World Shift** 로직 구현.

## ✅ 기대 효과
- **좌표 고정**: 쉐이더(Curved World)의 `World Position Offset` 연산이 원점 기준으로 항상 정확하게 적용됨.
- **완벽한 동기화**: 바닥이 캐릭터 속도와 1:1로 정확하게 반응. (캐릭터가 멈추면 바닥도 멈춤)
- **부드러운 이동**: 프레임 드랍이 있어도 캐릭터 이동량만큼만 바닥이 밀리므로 튀는 현상 없음.
