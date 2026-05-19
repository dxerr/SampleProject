# Spline 비균등 스케일 오프셋 오류 (Warp Target 왜곡)

## 키워드
Spline, Scale, Non-Uniform, Warp Target, Offset, Ledge, Traversal, UpdateSpline

## 증상
- 커브 구간 등에서 장애물(ExObstacle_Climb 등)의 좌우 폭(Y축) 스케일만 크게 늘어날 시(비균등 스케일 적용), 모션 워핑(Motion Warping) 시점의 목표 위치(Warp Target) 스플라인 계산이 심각하게 틀어짐.
- 시각적인 캐릭터 위치나 장애물 위치와 관계없이 Ledge 위치가 우측이나 특정 허공으로 고정되는 현상 (Snowballing 오프셋 왜곡).
- 에디터 배치(`LevelBlock_Traversable`)에서는 발생하지 않고, C++ 런타임 스폰 및 스케일 변경 시에만 발생함.

## 원인
- 언리얼 엔진의 `USplineComponent`는 비균등 스케일(Non-Uniform Scale) 적용 환경에서 거리를 기반으로 한 위치 계산 시 내부 좌표계 오차가 발생하는 자체 결함 특성이 있음.
- 특히 **런타임(Runtime)**에 `SetActorScale3D`를 사용하여 액터 스케일을 변경할 경우, 하위 `USplineComponent`의 곡선 길이와 내부 트랜스폼 캐시가 즉시 동기화되지 않고 이전 스케일 상태에 머무름.
- 결국 오염된 옛날 스플라인 캐시 정보 + 새롭게 늘어난 액터 매트릭스가 섞여서 거리 비례 위치 계산(Ex: `FindLocationClosestToWorldLocation`) 도중 터무니없는 좌표를 반환하게 됨.

## 해결 (우회 방안)
- 런타임에 액터 루트 스케일(Root Scale)을 비균등하게 변경했다면, **반드시 직후에 해당 액터 내부의 모든 `USplineComponent`를 찾아 `UpdateSpline()` 함수를 명시적으로 수동 호출**해야 함. 
- 이를 통해 엔진 모르게 어긋난 스플라인 곡선과 바운드(Bounding Box) 메타 데이터를 해당 프레임에 강제로 최신 동기화시켜 오프셋(Warp Target) 오류를 방지할 수 있음.

### 수정 코드 예시 (`ExObstacleStrategy_Climb.cpp`)
```cpp
// 1. 기존처럼 직관적으로 액터 전체 루트 비균등 스케일 적용
Obstacle->SetActorScale3D(FVector(ScaleX, ScaleY, ScaleZ));

// 2. [핵심 해결책] 런타임에 스케일 변경 시 Spline 내부 캐시 강제 갱신!
TArray<USplineComponent*> Splines;
Obstacle->GetComponents<USplineComponent>(Splines);
for (USplineComponent* Spline : Splines)
{
	Spline->UpdateSpline();
}
```

## 날짜
2026-02-24
