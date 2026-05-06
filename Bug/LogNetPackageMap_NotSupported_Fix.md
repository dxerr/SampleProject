# 버그 리포트: LogNetPackageMap NOT Supported (동적 생성 컴포넌트 네트워크 경고)

## 날짜
2026-04-27

## 현상 (Issue)
멀티플레이 환경에서 결정론적(Deterministic)으로 동일한 랜덤 시드를 바탕으로 서버와 클라이언트가 각자 `AExFloorChunk`를 스폰함.
해당 청크 내부에서 `NewObject<USplineMeshComponent>` 및 `NewObject<UBoxComponent>`를 통해 런타임에 동적으로 바닥 및 충돌체를 생성함.
이때 서버의 캐릭터(Mover)가 생성된 동적 컴포넌트를 밟으면, 서버가 클라이언트에게 이동 기반(MovementBase) 정보를 동기화하기 위해 해당 컴포넌트의 네트워크 고유 ID(NetGUID)를 찾음.
하지만 동적 생성 컴포넌트이므로 네트워크에서 서로 매칭하지 못하고 아래와 같은 경고가 대량으로 발생함.

`LogNetPackageMap: Warning: FNetGUIDCache::SupportsObject: SplineMeshComponent /ExRunnerPlay/Map/UEDPIE_0_L_ExRunnerTest.L_ExRunnerTest:PersistentLevel.BP_ExFloorChunk_C_2.CurveSpline_0 NOT Supported.`

## 원인 (Cause)
* Unreal Engine의 `NetGUIDCache`는 기본적으로 클래스 생성자(Constructor)에서 `CreateDefaultSubobject`로 생성된 컴포넌트들에 대해서만 '안정적 이름(Stable Name)'을 보장하여 네트워크 전송을 허용함.
* 런타임(`BeginPlay`나 기타 시점)에 `NewObject`로 생성된 컴포넌트는 기본적으로 서버와 클라이언트 간 이름 매칭이 보장되지 않으므로 네트워크 전송 객체로 지원되지 않음(NOT Supported).

## 해결방안 (Resolution)
동적으로 생성되는 컴포넌트(`SplineMeshComponent`, `BoxComponent` 등)에 대해 **`SetNetAddressable()`** 함수를 호출함.

### `SetNetAddressable()` 의 역할:
"이 컴포넌트는 비록 런타임에 동적으로 생성되었지만, 서버와 클라이언트 양쪽에서 완벽히 동일한 이름으로 생성될 것을 개발자인 내가 보장한다. 그러니 경고를 띄우지 말고 네트워크 이름(Stable Name)으로 취급해라!" 라는 의미를 언리얼 네트워크 시스템에 전달함.

### 적용 코드 예시:
```cpp
USplineMeshComponent* SplineMesh = NewObject<USplineMeshComponent>(this, SplineMeshName);
// ... 컴포넌트 설정 ...
// ▼ 언리얼 네트워크의 '결정론적 생성' 인증 마크
SplineMesh->SetNetAddressable();
SplineMesh->RegisterComponent();
```
마찬가지로 충돌체인 `BoxCollision`에도 적용하여 Mover 시스템이 동적 발판을 네트워크로 정상 전달/수신하도록 보완 완료.
