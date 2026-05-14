# OnlineSubsystemEOS (Project-Level Override)

이 폴더의 `OnlineSubsystemEOS` 플러그인은 에픽게임즈 공식 언리얼 엔진(Unreal Engine)에 내장된 원본 플러그인을 프로젝트 레벨로 복사하여 **오버라이드(Override)** 한 버전입니다.

## 1. 원본 위치
- **Original Path:** `[UnrealEngineRoot]/Engine/Plugins/Online/OnlineSubsystemEOS`
- **Engine Version:** 5.4 (기준)

## 2. 분리(Override) 배경
- **EOS Device ID 로그인 오류 해결:** PC(Listen Server/Dedicated Server) 환경에서 EOS 플랫폼(플랫폼 계정 연동 없이) 순수 Device ID만으로 로그인을 시도할 때, 엔진 내부적으로 `UserLoginInfo` 구조체가 세팅되지 않아 `EOS_InvalidParameters` 에러가 발생하는 치명적인 버그가 있었습니다.
- **커스텀 엔진 유지보수 방지:** 이 버그를 고치기 위해 엔진 원본 소스를 직접 수정하면, 팀원들의 로컬 환경이나 CI/CD 빌드 머신에서도 모두 커스텀 엔진을 받아 수 시간 동안 전체 엔진을 재빌드해야 하는 지옥이 펼쳐집니다. 
- **해결책:** 해당 플러그인만 프로젝트의 `Plugins/` 폴더로 그대로 복사해 오면, 언리얼 빌드 툴(UBT)이 자동으로 엔진 플러그인 대신 이 프로젝트 플러그인을 우선하여 로드합니다. 이를 통해 **팀원들은 순정 엔진을 그대로 사용하면서 빌드 시간을 단축**할 수 있습니다.

## 3. 소스코드 수정 내역
- **Target File:** `Source/OnlineSubsystemEOS/Private/UserManagerEOS.cpp`
- **Modification:**
  - `ConnectLoginEAS` 및 관련 함수 내부에 존재하던 `#if ADD_USER_LOGIN_INFO` 매크로 조건문을 삭제했습니다.
  - 이를 통해 특정 스위치(NSA) 콘솔 환경이 아닌 일반 PC 환경에서도 Device ID 로그인 시 `UserLoginInfo`가 항상 정상적으로 할당되도록 강제하여 `EOS_InvalidParameters` 에러를 우회합니다.
- **주석 마커:** 향후 추적을 용이하게 하기 위해 가급적 수정한 부분 주변에 `// [Ex] Modified` 주석을 함께 참고할 수 있습니다. (현재 버전은 매크로 조건문만 삭제된 상태입니다.)

## 4. 엔진 업데이트(버전업) 시 사후 처리 가이드 (매우 중요)
향후 프로젝트를 언리얼 5.5 등 새 버전으로 업그레이드할 경우 **주의가 필요합니다.** 프로젝트에 복사해 둔 이 플러그인은 자동으로 업데이트되지 않습니다.

1. 새 엔진 버전에 맞춰 `[UnrealEngineRoot]/Engine/Plugins/Online/OnlineSubsystemEOS` 폴더를 다시 이 프로젝트 폴더로 **통째로 덮어쓰기 복사**합니다.
2. 덮어쓰기 완료 후, `Source/OnlineSubsystemEOS/Private/UserManagerEOS.cpp`를 열어 **기존에 수행했던 `#if ADD_USER_LOGIN_INFO` 삭제 패치를 수동으로 다시 적용**해야 합니다.
3. 적용 후 `ExFrameWork.uproject`에서 `Generate Visual Studio project files`를 실행하고 재컴파일합니다.
