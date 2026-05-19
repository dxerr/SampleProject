# 안드로이드 빌드 ShaderMap 크래시 이슈 통합 정리

> **통합 일자**: 2026-06-xx  
> **원본 파일**: `Android_ShaderMap_TGPUSkin_Unlimited.md`, `Android_ShaderMap_SkinCache_Crash.md`, `Android_ShaderMap_Vulkan_Crash.md` (통합 후 삭제)  
> **최종 상태**: ✅ 전체 해결 완료  
> **적용 파일**: `Config/Android/AndroidEngine.ini`

---

## 공통 증상

안드로이드 기기에서 앱 실행 직후 UE 로고 이후 Fatal Error 크래시 발생.

```
Fatal error: [File:./Runtime/Engine/Private/Materials/MaterialShared.cpp]
Failed to find shader map for default material DefaultDeferredDecalMaterial!
LogMaterial: Warning: Incomplete material ... missing (...) from TGPUSkinVertexFactory[Unlimited|Default].
```

세 가지 별개 원인으로 같은 크래시 패턴이 반복되었으며, 각각 독립적으로 조사 및 해결됨.

---

## 원인 1: UnlimitedBoneInfluences CVar 오버라이드 실패

**증상 키워드**: `TGPUSkinVertexFactoryUnlimited`

**원인**
- `DefaultEngine.ini`에 `r.GPUSkin.UnlimitedBoneInfluences=True`가 전역 활성화.
- `AndroidEngine.ini`의 `[/Script/Engine.RendererSettings]`에서 `False` 문자열로 오버라이드 시도했으나, 플랫폼 INI 병합 정책 또는 문자열 파싱 문제로 기기에서 값이 `1`로 남아 있었음.
- 모바일 빌드에서 `TGPUSkinVertexFactoryUnlimited` 쿡 변종이 제외되지만, 기기 런타임은 이 변종을 요구하여 크래시 발생.

**해결**
`AndroidEngine.ini`의 `[SystemSettings]` 섹션에 명시적 정수 값으로 CVar 오버라이드:
```ini
[SystemSettings]
r.GPUSkin.UnlimitedBoneInfluences=0
```

---

## 원인 2: SkinCache CompileShaders 쿠커-런타임 불일치

**증상 키워드**: `TGPUSkinVertexFactoryDefault`

**원인**
- `DefaultEngine.ini`에 `r.SkinCache.CompileShaders=True` 설정.
- PC 쿠커는 스킨 캐시 전용 버텍스 팩토리만 컴파일하고 `TGPUSkinVertexFactoryDefault`는 최적화 목적으로 제거(Strip).
- 모바일 Vulkan/ES3.1 기기는 컴퓨트 셰이더 한계로 스킨 캐시를 사용 불가 → `Default` 변종으로 런타임 폴백(Fallback).
- 쿠커가 `Default` 변종을 굽지 않았는데 기기가 이를 요구 → 셰이더 맵 불일치 크래시.

**해결**
```ini
[SystemSettings]
r.SkinCache.CompileShaders=0
```

---

## 원인 3: Mobile Forward + Vulkan 엔진 아키텍처 버그

**증상 키워드**: `TGPUSkinVertexFactoryDefault`, `DefaultDeferredDecalMaterial`, Vulkan

**원인**
- `bBuildForVulkan=True` + `r.Mobile.ShadingPath=1` (Mobile Forward) 조합에서 발생하는 엔진 버그.
- Vulkan 환경에서 셰이더 컴파일러가 데칼 머티리얼의 스켈레탈 메시 변종 셰이더를 최적화 목적으로 강제 누락(Strip).
- 그러나 엔진 런타임 초기화 시 `DefaultDeferredDecalMaterial`의 `Used with Skeletal Mesh` 옵션 때문에 해당 셰이더 변종을 무조건 요구.
- **컴파일러는 제거, 런타임은 필수 요구** → 동작 불일치로 크래시 발생.
- `AndroidEngine.ini` 삭제 시 정상 작동했던 이유: Vulkan 강제 설정이 무효화되어 OpenGL ES3.1로 폴백되었기 때문.

**해결**
Vulkan은 유지하되 (`bBuildForVulkan=True`), 문제 원인인 셰이더 변종 요구를 CVar로 억제:
```ini
[SystemSettings]
r.GPUSkin.UnlimitedBoneInfluences=0
r.SkinCache.CompileShaders=0
r.Mobile.ShadingPath=0
```
> 더미 데칼 머티리얼 방식(옵션A-더미) 없이 CVar 방식만으로 해결 완료.  
> `r.Mobile.ShadingPath=0` (Mobile Deferred)로 전환하여 Unlit SkyBox StaticMesh 배경 패스 정상 렌더링 효과도 동시 확보.

---

## 최종 적용 INI 상태 (`Config/Android/AndroidEngine.ini` 기준)

```ini
[/Script/AndroidRuntimeSettings.AndroidRuntimeSettings]
bBuildForES31=False
bBuildForVulkan=True

[SystemSettings]
r.GPUSkin.UnlimitedBoneInfluences=0
r.SkinCache.CompileShaders=0
r.Mobile.ShadingPath=0
r.Streaming.LimitPoolSizeToVRAM=0
```

---

## 교훈 및 규칙

| 규칙 | 내용 |
|------|------|
| **CVar 오버라이드는 반드시 `[SystemSettings]` 섹션** | `[/Script/Engine.RendererSettings]`나 `[ConsoleVariables]`는 병합 우선순위 문제로 신뢰 불가. `[SystemSettings]`에 정수 값(`0`/`1`)으로 명시. |
| **Boolean 형식 금지** | `True`/`False` 문자열 대신 반드시 정수 `1`/`0` 사용. |
| **`bBuildForES31=False` 필수** | `True`로 두면 MetaHuman/Paragon 에셋 포함 시 ES3.1 셰이더 루프 무한 순환 발생. |
| **Vulkan + Mobile Forward 주의** | 두 설정의 조합은 데칼 머티리얼 관련 엔진 버그를 트리거함. `r.Mobile.ShadingPath=0` 유지 권장. |
