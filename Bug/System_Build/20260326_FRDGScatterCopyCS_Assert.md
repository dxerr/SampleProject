# 2026-03-26: 언리얼 에디터 FRDGScatterCopyCS 어설트 크래시 분석 (recompileshaders all 버그)

## ISSUE (현상)
- 언리얼 엔진 5(DirectX 12 SM6 환경) 에디터에서 `DerivedDataCache`(DDC, 캐시 폴더)를 지우고 프로젝트를 새로 켠 직후,
- 콘솔 명령어 `recompileshaders all`을 입력하면 에디터가 버티지 못하고 뻗어버리는 현상 발생.
- **크래시 로그 일부분**: `Assertion failed: Shader.IsValid() [File:C:\wz\UnrealEngine\Engine\Source\Runtime\RenderCore\Public\GlobalShader.h] [Line: 192] Failed to find shader type FRDGScatterCopyCS in Platform PCD3D_SM6`

## ROOT CAUSE (원인)
- 에디터 내 뷰포트나 프리뷰를 위해 작동 중인 Scene Culling 시스템(RDG-Render Dependency Graph)은 매 렌더링 프레임마다 메모리 캐링이나 버퍼 복사를 위해 `FRDGScatterCopyCS`라는 글로벌 컴퓨트 셰이더를 실시간으로 필수 요구합니다.
- 사용자가 `recompileshaders all` 커맨드를 날리면, 렌더링 스레드는 셰이더 컴파일 대기열(Queue)에 모든 셰이더를 넣고 현재 메모리 상의 셰이더 맵을 무효화(Invalidate)시킵니다.
- 그러나 해당 글로벌 셰이더의 *백그라운드 비동기 컴파일*이 미처 끝나기도 전에, 메인 렌더링 루프가 다음 프레임을 그리려 시도하면서 셰이더 맵을 조회합니다.
- 그 결과, 필수 컴파일이 아직 완료되지 않아 "Invalid"(아직 없음) 상태이므로 `Shader.IsValid()` 어설트에 걸려 엔진 기능이 강제 종료(Fatal Error)되는 **엔진 구조상 타이밍 버그**입니다.

## SOLUTION (해결 방안 / 우회책)
1. **`recompileshaders all` 사용 금지 주의**: DDC를 지우고 에디터를 켤 경우, 필요한 기본 셰이더들이 셰이더 컴파일러 워커를 통해 온전히 백그라운드 컴파일될 때까지 수동 명령어(전체 재컴파일) 입력을 자제해야 합니다.
2. **`recompileshaders changed` 사용**: 특정 셰이더(머티리얼)만 수정한 경우, `recompileshaders changed` (단축키 `Ctrl+Shift+.`)를 입력하여 전역 시스템의 셰이더를 건드리지 않고 변경된 내역만 컴파일해야 크래시를 방지할 수 있습니다.
3. **오프라인 셰이더 빌드 권장**: 전체 셰이더나 캐시를 명시적으로 깨끗하게 다시 구워야 할 때는, 에디터 자체를 띄우지 않고 커맨드라인 툴을 통해 렌더러 동작 없이 DDC를 굽는 방식을 사용하는 것이 안정적입니다 (`UnrealEditor-Cmd.exe -run=DerivedDataCache -fill`).
