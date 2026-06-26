# Horde 서버 IP 변경 방법 및 위치 가이드

## 이슈 요약
- 키워드: Horde, Unreal Build Accelerator, UBA, IP 변경, 분산 빌드, UBT
- 문제 현상: 분산 빌드에 사용되는 Horde 서버의 내부 IP(10.37.0.216 -> 10.13.0.63)가 변경되어 UBT가 이전 IP로 접근을 시도함.

## 원인
- 언리얼 엔진의 빌드 구성(UnrealBuildTool) 파일인 `BuildConfiguration.xml` 내에 기존 Horde 서버 URL(http://10.37.0.216:13340)이 하드코딩 또는 명시적으로 설정되어 있음.

## 해결 방법
해당 PC 및 다른 PC에서 다음 경로에 위치한 `BuildConfiguration.xml` 파일을 열어 IP를 변경합니다.

1. **설정 파일 위치:**
   - `%APPDATA%\Unreal Engine\UnrealBuildTool\BuildConfiguration.xml`
   - 또는 `%USERPROFILE%\Documents\Unreal Engine\UnrealBuildTool\BuildConfiguration.xml`

2. **변경 사항:**
   파일을 메모장 등으로 열고 `<Horde>` 노드 하위의 `<Server>` 값을 새로운 IP로 변경합니다.

   **변경 전:**
   ```xml
   <Horde>
     <Server>http://10.37.0.216:13340</Server>
     <WindowsPool>Win-UE5</WindowsPool>
   </Horde>
   ```

   **변경 후:**
   ```xml
   <Horde>
     <Server>http://10.13.0.63:13340</Server>
     <WindowsPool>Win-UE5</WindowsPool>
   </Horde>
   ```
