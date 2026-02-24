# Bug Report: 등반 장애물 오브젝트 풀링 복원 중 스플라인 무한 팽창 버그 (Snowballing Scale Bug)

## 이슈 개요
- **증상**: 러너 게임 중 가끔씩 커브 또는 직선 지점에서 장애물을 넘지 못하고 부딪히는 현상 발생. `Has Front Ledge` 등 라인 트레이스 결과는 표시되나 올바른 애니메이션(Hurdle, Vault 등)이 발동하지 않고 디버그 텍스트로 `None`이 출력됨 (스크린샷 참조). 
- **원인 추정**: 오브젝트 풀링을 거쳐 재사용(Restore)되는 과정에 문제가 있을 것이라는 직관적인 의심.

## 원인 분석 (Root Cause)

1. **오브젝트 풀링 '눈덩이 현상' 방지 코드의 순서 오류**
   - 현재 `ExObstacleStrategy_Climb.cpp`의 `ConfigureObstacle_Implementation` 코드에는 이전 스케일 백업 변수인 `PreviousScale`을 구하는 로직이 있습니다.
   - 문제는 ഈ 변수를 구하기 **직전**에 `Obstacle->SetActorScale3D(FVector::OneVector)`가 두 차례 반복 호출된다는 점입니다!

2. **RootComponent와 RelativeScale3D 덮어쓰기**
   - 만약 타겟 액터(`BP_ExObstacleClimb` 등)의 `StaticMeshComponent`가 **루트 컴포넌트(Root Component)** 역할을 한다면, `SetActorScale3D` 호출 즉시 그 안의 상대 스케일(`RelativeScale3D`)마저 1.0으로 리셋되어버립니다.
   - 따라서 해당 코드는 과거의 스케일이 어땠든 항상 `PreviousScale = 1.0, 1.0, 1.0`을 반환하게 됩니다.

3. **스플라인(Spline) 포인트 스케일 누적 왜곡 (Snowballing)**
   - 스케일 오차 방지 목적으로 만들어둔 `ScaleMultiplier = ScaleX / PreviousScale.X` 구문이 항상 `ScaleX / 1.0 = ScaleX`로 계산됩니다!
   - 하지만 스플라인 컴포넌트 내부의 로컬 위치값들은 리셋되지 않은 채 **이미 이전 소환 시점에서 곱해져 비대해진 상태**입니다.
   - 코드는 계속해서 비대해진 로컬 위치값(`OrigLocalPos`)에 또다시 새로운 `ScaleX`를 곱하므로, 풀링될 때마다 스플라인 포인트 좌표가 `100 -> 200 -> 400 -> 800` 식으로 기하급수적으로 폭발하게 됩니다.

4. **Traversal Logic의 오판 판단 (`None` 반환)**
   - 스플라인 포인트가 물리적 메시 크기를 한참 벗어나 저 멀리 허공으로 뻗어나감에 따라, `AC_TraversalLogic` 내 스플라인 기반 모션 검사 노드들이 벽이나 렛지의 위치를 전혀 엉뚱한(바닥이나 허공) 곳으로 인식하게 됩니다.
   - 엉뚱하게 측정한 지형(예: Back Ledge Height가 터무니없이 낮은 바닥)은 'Hurdle'이나 'Mantle' 등의 조건을 만족하지 않아 `None` (등반 액션 실패)으로 폐기되어, 캐릭터가 애니메이션 없이 그대로 벽을 파고드는 것입니다.

## 해결 방안 (Resolution)

간단하지만 확실한 위치 변경입니다:
- **`PreviousScale` 백업 시점을 액터 스케일 초기화(`SetActorScale3D`) 직전의 맨 윗줄로 끌어올립니다.**
이렇게 하면 액터 스케일이 리셋되기 전에 현재(재사용 전) 스태틱 메시의 실제 스케일을 안전하게 캡처하여, 스플라인 위치 배수(`ScaleMultiplier`)를 완벽하게 상쇄 연산할 수 있게 됩니다.

## 결론
주인님의 "풀링 복원(restore) 문제일 것 같다"는 촉이 100% 정확하게 적중했습니다. 스플라인 위치 왜곡이라는 고질적인 풀 관리 연산 버그를 찾아냈고, 이를 순서 변경 하나로 완전히 해결할 수 있습니다. 
