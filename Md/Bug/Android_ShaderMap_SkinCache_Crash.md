# 안드로이드 빌드 시 TGPUSkinVertexFactoryDefault 셰이더 맵 누락(Crash) 이슈

## 현상 (Issue)
`UnlimitedBoneInfluences` 설정 문제를 수정한 직후에도, 기기에서 `DefaultDeferredDecalMaterial`을 로드할 때 아래와 같은 동일한 패턴의 크래시가 발생함.
```
Fatal error: [File:./Runtime/Engine/Private/Materials/MaterialShared.cpp] [Line: 3006] 
Failed to find shader map for default material DefaultDeferredDecalMaterial(/Engine/EngineMaterials/DefaultDeferredDecalMaterial.DefaultDeferredDecalMaterial)! 
LogMaterial: Warning: Incomplete material DefaultDeferredDecalMaterial, missing (TMobileBasePassPSF..._LOCAL_LIGHTS_DISABLED, 0) from TGPUSkinVertexFactoryDefault.
```

## 원인 (Root Cause)
1. **스킨 캐시(Skin Cache) 전역 설정과 모바일 지원 한계의 충돌**:
   - `DefaultEngine.ini`에 `r.SkinCache.CompileShaders=True` 옵션이 켜져 있었습니다.
   - PC/쿠커(Cooker)는 스킨 캐시 전용 버텍스 팩토리만 컴파일하고 호환성이 떨어지는 기본 버텍스 팩토리(`TGPUSkinVertexFactoryDefault`)는 최적화를 위해 생성하지 않았습니다(Strip).
   - 반면 모바일(Vulkan ES3.1) 기기는 런타임 환경에서 컴퓨트 셰이더 한계로 스킨 캐시를 쓰지 못해 `Default` 버텍스 팩토리 사용으로 강제 폴백(Fallback)합니다.
   - 쿠커는 `Default` 버전을 굽지 않았는데 기기는 `Default` 버전을 달라고 엔진에 요구하여 셰이더 맵 불일치에 의한 크래시가 났습니다.

## 해결 (Resolution)
`AndroidEngine.ini`의 `[ConsoleVariables]` 섹션에 명시적으로 스킨 캐시 컴파일러 비활성화 옵션을 넣었습니다.
```ini
[ConsoleVariables]
r.SkinCache.CompileShaders=0
```
이로써 안드로이드 빌드용 쿠커가 강제로 모바일 `Default` 버텍스 팩토리를 구워내게 만들어 기기 폴백 요청 시 셰이더 맵을 즉시 로드할 수 있도록 수정했습니다.
