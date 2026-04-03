# 안드로이드 빌드 SkyBox 머터리얼 미적용 - 쿠커 경로 중복 이슈

## 현상 (Issue)
안드로이드 기기에서 SkyBox가 검정으로 표시되고, 빌드 로그에 아래 Warning이 출력됨.
```
LogCook: Warning: Unable to find package for cooking
/Game/Game/Atmospheric_Skybox/Materials/MasterMaterial/MasterMaterial.MasterMaterial.
Instigator: { CommandLineDirectory: ../../../../../SampleProject/Content/ }.
```

## 원인 (Root Cause)
`Content/Game/Atmospheric_Skybox/Materials/MasterMaterial/MasterMaterial.MasterMaterial.uasset`
경로에 MasterMaterial의 복제본이 잘못 커밋되어 있었음 (커밋 d78c637).

UE 쿠커는 Content 루트를 `/Game/`으로 매핑하므로,
`Content/Game/...` 경로는 `/Game/Game/...`으로 해석됨.
쿠커가 해당 경로의 패키지를 탐색하다 실패하면서 SkyBox 머터리얼이 쿠킹되지 않음.

실제 올바른 경로:
- Content 디스크: `Content/Atmospheric_Skybox/Materials/MasterMaterial/MasterMaterial.uasset`
- UE 에셋 경로: `/Game/Atmospheric_Skybox/Materials/MasterMaterial/MasterMaterial`

중복 파일 경로 (잘못됨):
- Content 디스크: `Content/Game/Atmospheric_Skybox/Materials/MasterMaterial/MasterMaterial.MasterMaterial.uasset`
- UE 에셋 경로: `/Game/Game/Atmospheric_Skybox/...` (에디터에서 인식 불가)

## 해결 (Resolution)
`git rm`으로 중복 파일 제거 후 커밋 (d4d1d8b).
```
git rm Content/Game/Atmospheric_Skybox/Materials/MasterMaterial/MasterMaterial.MasterMaterial.uasset
git commit -m "fix: remove duplicate MasterMaterial under Content/Game - cook path mismatch"
```

## 함께 적용된 설정 변경
- `DefaultEngine.ini`: `r.Mobile.ShadingPath=1` → `0` (Mobile Forward로 통일, 쿡 기준 일치)
- `MasterMaterial`: `is_sky=True` → `False` (StaticMesh SkyBox에 불필요, Android 렌더패스 충돌 방지)
- `SkyLight`: `RealTimeCapture=True` → `False` (모바일 미지원 기능 비활성화, 에디터 경고 제거)

## 추가 수정: Two Sided 백페이스 컬링 (2d443e5)

### 현상
Content/Game/ 중복 파일 제거 후에도 Android 기기에서 SkyBox 상단(하늘 부분)이 여전히 검정.
에디터 Vulkan 프리뷰에서는 정상 표시됨.

### 원인
SkyBox 메시(SkyBox.SkyBox) 바운딩 박스:
- Z 범위: 0 ~ +10000 (하단이 Z=0, 위쪽만 존재)
- 메시 법선이 외부를 향하는 구조

MasterMaterial의 `two_sided=False` 상태에서:
- PC/에디터: 백페이스 컬링이 관대하게 적용되어 렌더링됨
- Android Vulkan 모바일: 백페이스 컬링이 엄격하게 적용되어 안쪽에서 보이는 면이 전부 제거됨

결과적으로 카메라가 메시 내부에 있을 때 상단 폴리곤이 Vulkan 모바일에서 컬링으로 사라짐.

### 해결
MasterMaterial `two_sided=True` 설정.

## 교훈
- UE에서 `Content/Game/` 하위 폴더를 만들면 쿠커가 `/Game/Game/` 경로로 해석하여 탐색 실패
- 에디터 Content Browser에서 보이지 않는 파일이 git에 존재할 수 있으므로, Cook Warning의 경로가 이상할 경우 git ls-files로 실제 파일 존재 여부를 확인해야 함
