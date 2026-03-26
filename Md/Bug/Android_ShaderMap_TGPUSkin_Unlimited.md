# 안드로이드 빌드 시 TGPUSkinVertexFactoryUnlimited 셰이더 맵 관련 앱 종료(Crash) 이슈

## 현상 (Issue)
안드로이드 디바이스에서 앱 실행 시, Unreal Engine 로고 이후 즉각적인 앱 종료(Crash) 현상이 발생함.
Logcat 추출 결과 아래와 같은 Fatal Error 로그가 확인됨.
```
Fatal error: [File:./Runtime/Engine/Private/Materials/MaterialShared.cpp] [Line: 3006] 
Failed to find shader map for default material DefaultDeferredDecalMaterial(/Engine/EngineMaterials/DefaultDeferredDecalMaterial.DefaultDeferredDecalMaterial)! Please make sure cooking was successful (Contains inline shaders, has GTSM)
```
추가로 이 에러 발생 직전 확인된 Warning:
```
LogMaterial: Warning: Incomplete material DefaultDeferredDecalMaterial, missing (TMobileBasePassPSFMobileDirectionalLightAndSHIndirectPolicyLOCAL_LIGHTS_DISABLED, 0) from TGPUSkinVertexFactoryUnlimited.
```

## 원인 (Root Cause)
1. **무제한 본 웨이트(Unlimited Bone Influences) CVAR 오버라이드 실패**: 
    - `DefaultEngine.ini`에 `r.GPUSkin.UnlimitedBoneInfluences=True`가 전역으로 활성화되어 있었습니다.
    - 안드로이드 환경을 위해 `AndroidEngine.ini` 설정의 `[/Script/Engine.RendererSettings]`에서 `r.GPUSkin.UnlimitedBoneInfluences=False`로 오버라이드를 시도했으나, 런타임 로그 상 기기에서는 `Set CVar [[r.GPUSkin.UnlimitedBoneInfluences:1]]`로 적용되고 있었습니다.
    - 플랫폼별 INI 병합 정책이나 `False` 문자열 파싱 문제로 인해 `DefaultEngine.ini`의 `1` 설정이 이겼거나, ConsoleVariable은 `[/Script/Engine.RendererSettings]`로 오버라이드 되지 않은 것입니다.
2. **모바일 셰이더 변종 누락**: 
    - 모바일 빌드에서는 보통 성능 등의 이유로 `TGPUSkinVertexFactoryUnlimited` 변종(Permutation)이 최적화 파이프라인(Cook)에서 제외되거나 온전하게 구성되지 않습니다.
    - 그러나 런타임 디바이스에서는 CVar 값이 `1`로 평가되어 모바일 렌더러가 해당 무제한 본 웨이트 변종을 찾으려 시도했고, 쿠킹된 에셋에서 이를 찾을 수 없어서 크래시가 발생했습니다.

## 해결 (Resolution)
`AndroidEngine.ini` 파일에 `[ConsoleVariables]` 섹션을 신설하고 명시적인 숫자 `0`을 사용하여 CVAR 오버라이드를 강제 적용하였습니다. 또한 `[/Script/Engine.RendererSettings]`에도 문자가 아닌 정수(0)를 사용하도록 일괄 수정했습니다.
```ini
[ConsoleVariables]
r.GPUSkin.UnlimitedBoneInfluences=0
```
이를 통해 안드로이드 런타임에서 `UnlimitedBoneInfluences`가 비활성화된 상태로 부팅되어 존재하지 않는 셰이더 변종을 참조하지 않아 앱 로고 이후 정상적으로 실행되도록 조치하였습니다.
