# Horde Agent IP 불일치 문제

## 키워드
Horde, Agent, IP, 불일치, BuildConfiguration, agent.json, 레지스트리

## 증상
- Horde Server IP 변경 시 모든 Agent PC에서 수동으로 여러 위치의 IP를 변경해야 함
- `BuildConfiguration.xml`의 IP(`10.0.28.74`)와 `agent.json`/레지스트리(`10.28.0.74`)가 불일치할 수 있음
- 서비스 재시작 누락 시 설정 반영 안됨

## 영향 받는 설정 위치 (4곳)

| # | 파일/위치 | 경로 |
|---|-----------|------|
| 1 | agent.json | `C:\ProgramData\Epic\Horde\Agent\agent.json` |
| 2 | BuildConfiguration.xml | `%APPDATA%\Unreal Engine\UnrealBuildTool\BuildConfiguration.xml` |
| 3 | 레지스트리 HKLM | `HKLM\SOFTWARE\Epic Games\Horde\Url` |
| 4 | 레지스트리 HKCU | `HKCU\SOFTWARE\Epic Games\Horde\Url` |

## 해결
- `Tools\Setup_HordeAgent_IP.ps1` 일괄 변경 스크립트 제작
- 상단 `$NewIP`, `$NewPort` 변수만 변경 후 관리자 권한으로 실행
- 4곳 자동 검사 → 수정 → 교차 검증 → 서비스 재시작 포함

## 날짜
2026-02-23
