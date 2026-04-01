# [System] MCP Server Initialization Logic 통합 리포트

## 1. 개요
브라우저 기반 MCP 서버(`unreal-python-bridge`)를 초기화할 때 발생하는 경로 오류(`Path Error`)와 비정상 종료(`EOF Error`) 현상을 수정했습니다.

## 2. 주요 문제 및 원안 분석

### A. 경로 불일치 (`Path Mismatch`)
*   **증상**: MCP 서버 실행 시 `lyra_bridge_mcp.py` 파일을 찾지 못함.
*   **원인**: Antigravity 재설치 과정에서 기대 경로(`d:\wz\LyraStarterGame\Tools\MCP\`)와 실제 설치 경로(`c:\wz\ExFrameWork\Tools\MCP\`)가 일치하지 않음.
*   **해결**: `config.json`의 서버 바이너리 경로를 현재 프로젝트 경로에 맞춰 상대 경로로 수정.

### B. 초기화 실패 및 EOF 오류
*   **증상**: 서버 실행 직후 즉시 리턴되며 `process closed with EOF` 에러 발생.
*   **원인**: 필수 명령줄 인수(`--port`, `--host`) 미전달 및 Python 싱글톤 체크 실패.
*   **해결**:
    *   서버 실행 래퍼(Wrapper) 스크립트에 기본 포트(`8000`) 및 호스트 바인딩 로직 추가.
    *   중복 실행 방지를 위한 PID 파일 체크 로직 강화.

## 3. 핵심 수정 파일
*   `ExFrameWork/Tools/MCP/bridge_server.bat`
*   `Antigravity/config/mcp_servers.json`

## 4. 최종 결과
이제 MCP 서버는 프로젝트 경로와 상관없이 안정적으로 초기화되며, 언리얼 엔진과 AI 어시스턴트 간의 Python 브릿지 통신이 정상적으로 수행됩니다.
