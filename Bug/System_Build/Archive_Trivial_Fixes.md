# [System] Trivial Fixes Archive (사소한 수정 내역)

주인님, 아래 내역들은 기록의 가치가 낮거나 단순한 실수로 판명된 사소한 수정 사항들의 요약입니다. 폴더의 정갈함을 위해 본 파일에 통합 보관합니다.

| 일자 | 이슈명 | 원인 및 해결 요약 |
| :--- | :--- | :--- |
| 2026-02-26 | TickComponent Missing Header | `TickComponent` 함수 구현 시 세미콜론이나 중괄호 누락으로 인한 컴파일 에러 수정. |
| 2026-03-23 | SpawnTable Ensure Bug | `SpawnTable`이 유효하지 않을 때 발생하는 `ensureAlwaysMsgf` 어설션을 단순 `if` 체크로 방어 로직 강화. |
| 2026-03-31 | MCP Server Path Error | Antigravity 재설치 후 Python bridge 경로가 구버전(`d:/wz/...`)으로 남아있던 것을 현 프로젝트 경로로 수정. |
| 2026-04-01 | CompileError InputComponent | `BeginPlay` 중복 선언 및 불필요한 헤더 포함 관계 정리로 빌드 에러 해결. |
| 2026-04-01 | Yaw Limit Compile Fix | 모듈 간 참조 관계(`PublicDependencyModuleNames`) 누락으로 인한 링킹 에러 수정. |

---
*본 파일은 /Bug 폴더의 공간 확보를 위해 상세 리포트를 대체합니다.*
