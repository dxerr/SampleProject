# 안드로이드 Shipping 크래시 (Vulkan Adreno) 버그 리포트

## 개요
- **키워드:** #Shipping #Android #Vulkan #Adreno #Crash #vkCmdBindPipeline
- **현상:** Development 환경에서는 발생하지 않으나 Shipping 환경에서 구동(도는 렌더링 초기화) 도중 `vkCmdBindPipeline` 실패로 인한 크래시 발생.
- **콜스택 핵심:** `FVulkanGraphicsPipelineState::Bind` -> `qglinternal::vkCmdBindPipeline`

## 원인 분석 (Troubleshooting)
이 크래시는 안드로이드 Adreno GPU 계열에서 개발 중인 언리얼 엔진 5(Vulkan RHI)가 겪는 가장 악명 높은 드라이버 충돌(Crash) 중 하나입니다.
Development 환경에서는 디버깅 셰이더와 느슨한 파이프라인 컴파일이 허용되지만, Shipping 빌드에서는 컴파일 최적화 및 캐싱이 엄격해지면서 드라이버가 뻗어버립니다.

**주요 추정 원인:**
1. **PSO (Pipeline State Object) 캐시 오염:** 에픽게임즈의 PSO Caching 기능 혹은 로컬 중간 캐시 폴더의 캐시 데이터가 꼬인 상태로 패키징되어, 모바일 디바이스에서 로드할 때 바인딩 에러를 뿜음.
2. **MSAA 및 렌더링 피처 충돌:** Adreno 디바이스에서 Vulkan RHI가 특정 레벨의 MSAA(다중 샘플링 안티앨리어싱)나 스카이박스 용 특수 Unlit Material 등을 렌더링할 때 드라이버 버그를 일으킵니다.

## 해결 및 디버깅 전략 (Resolution)
1. **Saved / Intermediate 폴더 청소:** 에디터 외부에서 완벽히 프로젝트 리빌딩 및 캐시 클리어 수행.
2. **Project Settings 최적화 점검:**
   - 모바일 MSAA 설정 하향 (예: 2x 또는 None 설정)
   - 모바일 셰이더 프로파일 캐싱 임시 비활성화 (`r.ShaderPipelineCache.Enabled=0` 테스트)
3. **가장 중요한 옵션: 'Test' 빌드로 디버깅:**
   - Shipping 빌드는 로그(Logcat)를 완전히 무시하므로 원인파악이 매우 힘듭니다. 
   - 패키징 설정을 **Test 빌드**로 변경해서 먼저 원인 셰이더/머티리얼 정보를 완전히 잡아내야 합니다. Test 빌드는 Shipping의 최적화 레벨을 그대로 유지하면서 로깅 기능만 켜줍니다!
