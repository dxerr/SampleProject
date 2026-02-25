# Slide 장애물 및 곡선지대 Z축 Offset(들뜸/가라앉음) 버그 수정

## 개요 (Overview)
- **키워드:** #Slide #Obstacle #CurvedFloor #ZOffset #AExFloorChunk #GetLocalTransformAtDistance
- **현상:** 곡선(Curve)이나 경사로, 혹은 곡선/직선 연결 부위(Transition)에서 슬라이드(Slide) 장애물이 요구 높이보다 지나치게 높게 뜬 상태(Z=175 등)로 생성되거나, 측면에 배치되었을 때 오히려 바닥을 파고드는 현상이 발생함.
- **요구사항 파악:** 캐릭터의 캡슐을 우회하여 슬라이딩으로 통과할 수 있는 정확한 높이(130 근처 등 데이터 주도값)를 보장해야 함.

## 원인 분석 (Troubleshooting Process)

1. **AExFloorChunk 코어 함수 정렬 불일치 (원인 1)**
   - `ApplyCurve` 함수를 통해 생성되는 3D 메쉬(Spline Mesh)는 시작점이 아닌 정중앙을 원점(0)으로 기준으로 하여 `-HalfLength ~ +HalfLength` (혹은 각도 기준 `-HalfTotalAngle ~ +HalfTotalAngle`) 의 구간으로 곡선을 뒤틀어 생성됨. (Z의 경우에도 `-0.5 * Height` 지정)
   - 하지만 논리적인 좌표를 계산해주는 `GetLocalTransformAtDistance` 함수는 중심점 설계가 누락된 채 0 ~ Length(100%)까지를 단순히 선형 보간하는 낡은 수학 모델을 사용 중이었음.
   - 이로 인해 로직 상의 좌표계와 실제 그려진 메쉬의 좌표계가 최대 +0.5배(약 45 Unit 내외) 만큼 통짜로 빗나가는 Z 및 XY 들뜸/왜곡을 초래함.

2. **장애물 SpawnStrategy에서의 측면 Offset 방식 오류 (원인 2)**
   - Y Offset (가장자리 이동)을 부여할 때, "아직 기울기(Pitch)가 반영되지 않은 기본 회전값의 평면적 우측(RightVector)"으로 장애물을 그냥 밀어버린 뒤에야 기울기를 계산함.
   - 뱅킹(Banking)이나 경사면에 의해 가장자리가 원래 좌표의 수직 높이와 크게 달라지는 기하학적 특성이 적용되지 않아, 지형 높이차를 파고들거나 허공에 뜨는 결과가 발생함.

3. **Crouch Height의 하드코딩 의존 현상 (원인 3)**
   - 슬라이드의 통과 높이(PassHeight)를 사용자가 `UExObstacleDefinition` 데이터(DA)에서 MinSize/MaxSize로 설정했음에도 불구하고, 로직 내부에서 강제로 `GetCrouchPassHeight` (캐릭터의 Crouch Capsule 높이 + Margin)으로 덮어씌워 130 근처로 강제 락(Lock)을 걸어버림.

## 해결 방법 (Resolution)

1. **GetLocalTransformAtDistance 수학 모델 완벽 동기화**
   - X 좌표 및 각도 산출을 `-HalfLength` / `-HalfTotalAngle` 기준으로 재수립.
   - 높이(Z) 보간 역시 `(HeightOffset * Alpha) - (HeightOffset * 0.5f)`로 변경하여 `ApplyCurve` 메쉬 파이프라인과 싱크 100% 일치.

2. **기하학적 1-2-3 Transform 순서 재조정**
   - 중심점 위치 `LocalDist` 기준점 Z축 Location 획득 로직을 1순위로 조정.
   - 중심점에서 앞/뒤를 향해 던진 현(Chord) 벡터를 통해 `Pitch, Yaw` 회전값을 2순위로 수립 및 완전 적용.
   - *이후에* 각도와 경사가 완벽히 반영된 상태의 바뀐 축(Axis) 기준 RightVector를 타고 Y축 Offset을 밀어줌으로써 경사면을 따라 평행 이동하도록 수정.

3. **데이터 중심 설계(Data-Driven) 적용 완료**
   - 불필요한 `GetCrouchPassHeight` 캡슐 의존성을 완전 삭제.
   - `UExObstacleStrategy_Slide`에서 슬라이드(빈 통과 여백 공간)를 `Def->MinSize.Z ~ MaxSize.Z`에서 랜덤으로 그대로 직수입하여 사용하도록 수정.
   - 물리적인 블럭 메쉬 자체의 두께(Scale.Z)는 역할을 바꿔 `1.0 ~ 1.5` 배의 랜덤 난수로 고정하여 역할 분리.
