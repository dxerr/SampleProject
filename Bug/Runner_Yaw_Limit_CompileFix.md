# 러너 플레이 런타임 컴파일 에러 수정

## 원인
1. `UExGameModeDataSet` 클래스가 속한 `ExFrameWork` 모듈이 `ExRunnerPlayRuntime` 플러그인 모듈의 종속성에 포함되어 있지 않아서 `#include "Data/Modes/ExGameModeDataSet.h"` 구문 오류와 UHT 코드 생성 오류(`EXFRAMEWORK_API` 매크로 미인식)가 발생했습니다.
2. `ExFrameWork` 모듈 자체가 자신의 디렉터리를 `PublicIncludePaths`로 노출하지 않아, 외부 모듈(`ExRunnerPlayRuntime`)에서 포함 경로를 정확히 찾지 못하는 문제가 연달아 발생했습니다.

## 해결 방법
1. **플러그인 종속성 추가**: `c:\wz\ExFrameWork\Plugins\GameFeatures\ExRunnerPlay\Source\ExRunnerPlayRuntime\ExRunnerPlayRuntime.Build.cs` 파일을 수정하여 `PublicDependencyModuleNames` 목록에 `"ExFrameWork"`을 명시적으로 추가했습니다.
2. **모듈 Include Path 노출**: `c:\wz\ExFrameWork\Source\ExFrameWork\ExFrameWork.Build.cs` 파일에 `PublicIncludePaths.AddRange(new string[] { ModuleDirectory });` 구문을 추가하여, `ExFrameWork` 모듈에 의존하는 다른 모듈들이 그 안의 헤더(`Data/Modes/...`)를 문제없이 찾을 수 있도록 조치했습니다.

