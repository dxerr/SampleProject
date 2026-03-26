# 안드로이드 빌드 최적화(Vulkan) 시 TGPUSkinVertexFactoryDefault 셰이더 누락 크래시 원인 분석 및 내일 작업 대기 상태

## 1. 현상 (Issue) 요약
- 안드로이드 디바이스에서 앱 실행 직후 `Failed to find shader map for default material DefaultDeferredDecalMaterial... missing (TMobileBasePassPS..., 0) from TGPUSkinVertexFactoryDefault` 메시지와 함께 Fatal Error 크래시가 발생하는 고질적인 문제.
- **주요 단서**: `AndroidEngine.ini` 파일을 **삭제하면** 정상 작동(빌드 및 실행 성공)함을 확인함.

## 2. 완벽한 원인 분석 (Root Cause - Engine Bug)
- **Vulkan API 강제와 OpenGL 폴백**: `AndroidEngine.ini` 삭제 시 기존 INI에 있던 `bBuildForVulkan=True` (Vulkan 강제) 설정이 무효화되며 엔진 기본값인 **OpenGL ES 3.1**로 API가 폴백(Fallback)되어 오류가 나지 않았던 것입니다.
- **언리얼 엔진 5 모바일 포워드 + Vulkan의 버그**: 
  - `DefaultEngine.ini`에 의해 모바일 포워드 렌더러(`r.Mobile.ShadingPath=1`)가 활성화되어 있습니다.
  - Vulkan 환경 하에서 엔진 셰이더 컴파일러는 최적화를 목적으로 데칼 머티리얼에 대한 스켈레탈 메시 변종 셰이더(`TGPUSkinVertexFactory`) 생성을 강제로 완전히 누락(`Strip`)시킵니다.
  - 그러나 엔진 런타임 초기화 단계에서는 공용 기본 머티리얼인 `DefaultDeferredDecalMaterial`을 로드하면서, 이 텍스처 속성에 켜져 있는 `Used with Skeletal Mesh` 옵션을 보고 "스켈레탈 메시용 셰이더 맵"을 무조건 요구합니다.
  - 즉, **컴파일러는 지워버렸는데 런타임은 필수불가결로 요구**하는 동작 불일치로 인해 발생하는 **Vulkan 전용 엔진 아키텍처 버그**입니다. (과거에 의심했던 SkinCache나 UnlimitedBoneInfluences 등의 설정은 원인이 아니었기에 INI에서 롤백 처리 완료함)

## 3. 내일(Next Steps) 진행할 해결 방안 옵션 대기
주인님께서 내일 재부팅 후 어떠한 API 방향으로 갈지 결정해 주시면 다음 작업을 진행합니다:

### [옵션 A] 우회책 적용 (Vulkan 퍼포먼스 그대로 유지)
- **방법**: 프로젝트 애셋으로 내용이 빈 커스텀 데칼 머티리얼(`M_DummyDecal`)을 하나 생성하고 (단, `Used with Skeletal Mesh` 체크 해제), `DefaultEngine.ini`의 `DefaultDeferredDecalMaterialName` 옵션을 이 더미로 덮어씌워 엔진의 무결성 검증 버그를 회피합니다.
- **필요 작업**: 주인님께서 언리얼 에디터에서 해당 머티리얼을 1분 만에 생성해 주시거나, 자동화 파이썬 스크립트를 작성하여 실행.

### [옵션 B] 안정성 우선 (OpenGL ES 3.1로 API 완전 회귀)
- **방법**: Vulkan이 굳이 필요하지 않다면, 오류가 없는 OpenGL ES 3.1 렌더러로 공식 기조를 되돌립니다.
- **필요 작업**: `AndroidEngine.ini`에서 Vulkan 강제를 끄고 ES3.1을 활성화 (`bBuildForES31=True`, `bBuildForVulkan=False`).

---
> **기록 일시**: 2026-03-26 오전 03:15
> **현 상태**: 주인님의 내일 작업 지시(옵션 선택) 대기 중.
