# 머터리얼 Spline Mesh 미적용 버그

## 키워드
`Material`, `SplineMesh`, `Usage Flag`, `게임플레이 렌더링`, `bUsedWithSplineMeshes`

## 증상
- 머터리얼을 Spline Mesh Component에 적용 시, 에디터 프리뷰에서는 정상이나 **게임플레이(PIE) 시 기본 머터리얼(회색)로 대체**됨

## 원인
- UE5 머터리얼에 `Used with Spline Meshes` (MATUSAGE_SPLINE_MESH) 플래그가 비활성화 상태
- 셰이더 컴파일 시 Spline Mesh 변형용 셰이더 variant가 생성되지 않음

## 해결 방법

### Python 스크립트
```python
import unreal
mel = unreal.MaterialEditingLibrary
mat = unreal.EditorAssetLibrary.load_asset("/Game/path/to/Material")
mel.set_material_usage(mat, unreal.MaterialUsage.MATUSAGE_SPLINE_MESH)
unreal.EditorAssetLibrary.save_asset("/Game/path/to/Material")
```

### 에디터 UI
1. 머터리얼 에디터 열기
2. 좌측 **디테일 패널** → **사용(Usage)** 섹션
3. `Used with Spline Meshes` 체크박스 활성화
4. 저장

## 주의사항
- `MaterialEditingLibrary.set_material_usage()`는 2인자(material, usage)만 받음 (3인자 전달 시 에러)
- `set_material_usage()` 직후 같은 스크립트에서 `recompile_material()` 호출 시 에셋 손상 가능 → 별도 스크립트에서 리컴파일 권장
