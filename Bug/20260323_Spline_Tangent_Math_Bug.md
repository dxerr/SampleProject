# [2026-03-23] 스플라인 곡선(Curve) 단면의 Z축 높이 계산(Tangent) 수학적 오류 수정

## 현상 (증상)
- 곡선(Curve) 구간에 배치되는 코인(아이템)들이 하늘을 향해 평평하게 누워버림 (Pitch 회전값이 비정상적으로 약 90도에 수렴, Ex: 89.7도).
- 코인의 정상 배치 회전값인 Pitch 0도와 대조적으로 비정상적인 회전을 보임.

## 원인 분석
- 원인은 `ExFloorChunk::GetLocalTransformAtDistance` 함수 내에서 곡선(Curve) 스플라인의 **접선(Tangent) 벡터**를 계산하는 로직에 있었습니다.
- 기존 코드:
  ```cpp
  FVector WorldTangent = FVector::CrossProduct(...).GetSafeNormal() * DirSign;
  FVector LocalTangent = FVector(WorldTangent.X / ParentsScale.X, ...);
  if (!FMath::IsNearlyZero(CachedHeightOffset)) {
      LocalTangent.Z = CachedHeightOffset / ParentsScale.Z;
  }
  ```
- 문제점: `WorldTangent`는 `GetSafeNormal()`을 통해 **길이가 1인 단위 벡터(Unit Vector)**로 정규화됩니다. 이 단위 벡터의 Z축 값에, 월드 고도차를 나타내는 `CachedHeightOffset` (예: 50.0 등)을 그대로 대입했습니다.
- 결과: XY 평면 길이(1 미만)에 비해 Z축 길이가 비정상적으로 거대해져(예: X=0.7, Y=0.7, Z=50.0), `LocalTangent.Rotation()` 계산 시 Pitch가 почти 90도(89.7도 등)로 계산되었습니다.

## 해결 방법 (수정 내용)
- 단위 길이(길이 1)에 대한 Z축 높이 변화량, 즉 **기울기(Slope)**를 대입하도록 수정했습니다.
  ```cpp
  float Slope = CachedHeightOffset / ChunkLength;
  LocalTangent.Z = Slope / ParentsScale.Z;
  ```
- 변경 후 `LocalTangent.Z`는 전체 청크 길이에 비례한 단위 기울기를 가지게 되어, 경사진 곡선에서도 Pitch가 정상적인 한 자릿수(예: 2~5도)로 정확하게 계산됩니다.

## 결과
- `ExRunnerItemManager`의 `FTransform` 연산이 의도한 대로 완벽히 바닥 정렬 각도(Pitch 0도 근처)를 반환하게 되었습니다.
- 코인 BP(`BP_ExItemPickup_Coin`) 내부 Static Mesh의 `-90`도 Pitch 세팅과 결합되어, 코인이 바닥에 밀착한 상태로 정확히 수직으로 서 있게 됩니다.
