# 2026-03-26: 안드로이드 Global Shader 및 Cooked Content 누락 에러 분석

## ISSUE (현상)
- 렌더링 API를 Vulkan에서 OpenGL ES 3.1로 바꾸기 위해 INI 값을 수정한 후 안드로이드 빌드(패키징)를 진행하여 기기에 실행.
- 실행 직후 까만 화면에 다음과 같은 팝업 오류 발생 후 앱 구동 불가:
  `Game files required to initialize the global shader and cooked content are most likely missing. Refer to Engine log for details.`

## ROOT CAUSE (원인)
1. **렌더링 환경 급변에 따른 엔진 쿠커(Cooker)의 캐시 꼬임**:
   - 기존 Vulkan 환경에서 패키징(Cook)했던 데이터 캐시(DDC / Cooked 폴더)가 남아있는 상태에서 타겟 렌더러만 ES 3.1로 바꾼 채 증분 빌드(Incremental Build) 시도.
   - 언리얼 엔진 쿠커는 빌드 속도를 높이려고 이전 해시들을 재활용하려다, ES 3.1 타겟에 필요한 '필수 글로벌 셰이더(Global Shaders)'마저 APK 안에 온전히 패키징하지 않고 누락해버리는 치명적 꼬임 발생.
2. **INI 중복 설정 충돌 가능성**:
   - 앞서 `AndroidEngine.ini`는 수정했으나, `DefaultEngine.ini`에도 안드로이드 Vulkan 강제가 이중으로 덮어치듯 남아 있어 엔진이 안드로이드 쿠킹 시 충돌을 일으켰을 확률 높음.

## SOLUTION (해결 방안)
1. **이중 설정 통일 (조치 완료)**: `DefaultEngine.ini`의 `[/Script/AndroidRuntimeSettings.AndroidRuntimeSettings]`에도 `bBuildForVulkan=False`, `bBuildForES31=True`로 동기화 완료시켜 엔진 혼동 제거.
2. **캐시 완전 초기화 후 전체 리빌드 (Clean Build)**:
   - 프로젝트 안의 빌드 잔재물인 `Intermediate`, `Saved` 폴더(특히 `Saved/Cooked/Android~`)를 통째로 삭제.
   - 엔진 글로벌 셰이더 찌꺼기인 `C:\Users\[사용자명]\AppData\Local\UnrealEngine\Common\DerivedDataCache` 데이터 삭제.
   - 이후 에디터를 새로 열고 초기 셰이더 컴파일 대기 뒤 처음부터 다시 깔끔하게 전체 안드로이드 패키징을 돌리면 쿠커가 올바른 글로벌 셰이더를 탑재합니다.
