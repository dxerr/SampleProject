# 장애물 Y축 스케일 불일치 (커브 청크)

## 키워드
Obstacle, Scale, Y축, 폭, Width, FloorChunk, Curve, Bounds, AABB, GetVisualBoundsOf

## 증상
- 커브 바닥 청크 위의 장애물 오브젝트 Y축(폭) 스케일이 바닥과 불일치
- 직선 청크에서는 정상, 커브 청크에서만 발생

## 원인
- `ConfigureObstacle_Implementation`에서 `GetVisualBoundsOf(Chunk)` → `Mesh->Bounds`(월드 AABB) 사용
- 월드 AABB는 액터 회전에 영향받아, 커브 청크 회전 시 실제 바닥 폭과 다른 값 반환
- 예: 45도 회전된 직사각형 → AABB Y값이 실제 Y폭보다 커짐

## 해결
- 베이스 클래스 `UExObstacleSpawnStrategy`에 `GetFloorWidth` static 함수 추가
- `FloorMesh->GetStaticMesh()->GetBounds()` (로컬 Bounds) × `FloorMesh 스케일` 사용
- 회전에 영향받지 않는 정확한 폭 계산

### 수정 파일
| 파일 | 변경 내용 |
|------|-----------|
| `ExObstacleSpawnStrategy.h` | `GetFloorWidth` static 함수 선언 추가 |
| `ExObstacleSpawnStrategy.cpp` | `GetFloorWidth` 구현 + ConfigureObstacle/CalculateSpawnPosition 수정 |
| `ExObstacleStrategy_Climb.cpp` | 바닥 폭 계산 교체 |
| `ExObstacleStrategy_Slide.cpp` | 바닥 폭 계산 교체 (2곳) |
| `ExObstacleStrategy_Gap.cpp` | 바닥 폭 계산 교체 |
| `ExObstacleStrategy_WallRun.cpp` | 바닥 폭 계산 교체 |

## 날짜
2026-02-23
