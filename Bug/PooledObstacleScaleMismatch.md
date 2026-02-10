# 풀 재활용 장애물 스케일 불일치

## 키워드
GetActorBounds, BoxComponent, ObjectPool, Scale, Bounds, StaticMeshComponent

## 문제
풀에서 재활용된 장애물 오브젝트의 스케일이 올바르게 적용되지 않음.
- 첫 스폰 시 정상 → 풀 반환 → 재스폰 시 크기 불일치

## 원인
- `ExObstacleInteractionComponent`(`UBoxComponent` 상속)의 `BoxExtent`가 이전 스폰에서 변경된 값을 유지
- `SetActorScale3D(OneVector)` 호출해도 **컴포넌트의 BoxExtent는 리셋되지 않음**
- `GetActorBounds(true)` = Collision 기반 → 확대된 BoxExtent 포함하여 잘못된 BaseSize 반환
- 잘못된 BaseSize로 스케일을 계산하면 장애물이 너무 작거나 크게 생성됨

## 해결
- `GetActorBounds(true)` 대신 `StaticMeshComponent::Bounds`를 직접 사용 (기존 `GetVisualBounds` 로직과 동일)
- Collision 컴포넌트의 잔존 Extent 영향을 받지 않음
- `ActivateObstacle`에서 스케일/회전 초기화 + `UpdateComponentTransforms()` 추가

## 교훈
- UE 오브젝트 풀링 시 Actor 스케일 리셋만으로는 하위 컴포넌트(BoxComponent 등)의 상태가 초기화되지 않음
- Bounds 측정 시 Collision 기반(`GetActorBounds(true)`) vs Visual 기반(`Mesh->Bounds`) 차이 주의
