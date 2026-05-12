# Mover 컴포넌트: MovementModes 구조 및 동작 분석

> 작성일: 2026-05-12
> 대상 컴포넌트: UMoverComponent (UE5 차세대 이동 플러그인)

## 1. MovementModes의 실제 구조
*   **오해**: 에디터 내 디테일 패널에서 마치 블루프린트 커스텀 구조체(Struct)나 배열처럼 보여 BP 전용 데이터로 오해하기 쉽습니다.
*   **실제 (C++)**: `UMoverComponent` 내부에 정의된 `TMap<FName, TObjectPtr<UBaseMovementMode>>` 프로퍼티입니다. 즉, 상태 이름(FName)을 키(Key)로 하고, 해당 이동 모드에 대한 C++ 인스턴스 객체(Instanced Object)를 값(Value)으로 가지는 데이터 사전(Map)입니다.

## 2. 이동 속도 제어 구조 (MaxSpeed 등)
*   **데이터 위치**: 속도 데이터는 구조체를 직접 분해(Break)해서 가져오지 않습니다. "Walking" 같은 각 모드 내부에 할당된 **`Shared Settings` (공유 세팅 객체)** 안에 존재합니다.
    *   Ex) `Walking` -> `Shared Settings` -> `CommonLegacyMovementSettings` -> `MaxSpeed`
*   **접근 및 수정 방법**:
    *   **BP**: `CharacterMover` 객체에서 **`Find Shared Settings`** 노드를 호출해 `CommonLegacyMovementSettings` 객체를 얻어낸 뒤 값을 읽거나 씁니다.
    *   **C++**: `MoverComp->FindSharedSettings<UCommonLegacyMovementSettings>()`를 통해 포인터를 얻어 수정합니다.

## 3. 상태 치환 (Walking <-> Falling 등)의 발생 위치
Mover는 분기(Branch)문 기반이 아니라 상태 머신(State Machine)에 의해 그룹 전환을 관리합니다. 크게 3곳에서 처리됩니다.

1.  **엔진 내부 자동 전환 (내장 C++ 로직)**
    *   `UWalkingMode::OnGenerateMove` 등에서 바닥(Floor) 검사를 수행하고, 바닥이 없어지면 엔진이 스스로 `"Falling"` 모드로 큐잉(QueueNextMode)합니다. 
2.  **데이터 기반 전환 (Transitions 배열)**
    *   `CharacterMover`의 하단 `Transitions` 에디터 배열에 룰(Rule)을 추가하여, 조건(입력, 속도 등) 만족 시 다른 모드로 넘어가도록 비주얼 기반 설정이 가능합니다.
3.  **명시적 코드/블루프린트 강제 전환**
    *   원하는 시점에 코드로 강제 전환할 수 있습니다.
    *   **BP**: `Queue Next Mode` 노드에 상태 이름 입력.
    *   **C++**: `MoverComp->QueueNextMode(FName("Walking"));`

---
*참고: 위 내용을 바탕으로 속도 제어를 억지로 덮어쓰거나 Tick에서 강제 전환하지 않고, Mover의 의도된 흐름(Shared Settings 조회 및 Queue Next Mode)을 활용하는 것이 안정적입니다.*
