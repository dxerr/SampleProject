# DataDrivenCVars 및 DDCVar.PawnClass 분석 보고서

사용자님의 질문("DDCVar.PawnClass를 설정하는 에셋이나 방식")에 대한 정밀 분석 결과입니다.

## 1. DDCvar.PawnClass의 정체
이 변수는 C++ 코드가 아닌, **언리얼 엔진의 `DataDrivenCVars` 플러그인** 기능을 통해 **설정 파일(Config)**에서 정의된 콘솔 변수(Console Variable)입니다.

### 정의된 위치
*   **파일**: `Config/DefaultEngine.ini`
*   **섹션**: `[/Script/Engine.DataDrivenConsoleVariableSettings]`
*   **내용**:
    ```ini
    +CVarsArray=(Type=CVarInt,Name="DDCvar.PawnClass",ToolTip="",DefaultValueFloat=0.000000,DefaultValueInt=-1,DefaultValueBool=False)
    ```

### 의미
*   **Type=CVarInt**: 정수형(Integer) 변수입니다.
*   **DefaultValueInt=-1**: 기본값은 `-1`입니다.
*   이 설정에 의해 엔전이 시작될 때 `DDCvar.PawnClass`라는 콘솔 명령어가 자동으로 생성됩니다.

---

## 2. PawnClass 교체 방식 (GM_Sandbox)

`GM_Sandbox`는 이 콘솔 변수의 **숫자 값(Int)**을 읽어서, 그 숫자에 매핑된 **캐릭터 블루프린트 클래스**를 선택하는 방식을 사용합니다.

### 데이터 흐름 분석
1.  **참조 확인**: `GM_Sandbox`는 `BP_Twinblast`, `BP_Echo`, `BP_Manny` 등의 캐릭터 블루프린트를 **직접 참조(Dependencies)**하고 있습니다. (Data Asset을 경유하지 않음)
2.  **로직 추론**: 별도의 배열(Array) 변수가 노출되지 않은 것으로 보아, `GM_Sandbox`의 **Event Graph** 내부에 다음과 같은 **Select** 노드 형태의 로직이 하드코딩되어 있을 것입니다.

    ```mermaid
    graph LR
        A[Get Console Variable Int<br/>(DDCvar.PawnClass)] --> B{Select / Switch}
        B -->|0| C[BP_Manny]
        B -->|1| D[BP_Twinblast]
        B -->|2| E[BP_Echo]
        B -->|Default| F[SandboxCharacter_CMC]
        B --> G[Spawn Actor]
    ```

### 결론
*   **"설정하는 에셋"**: 캐릭터 목록을 관리하는 별도의 Data Asset은 **없으며**, `DefaultEngine.ini`가 변수를 정의하고, `GM_Sandbox` 블루프린트 내부의 노드 흐름이 매핑을 담당합니다.
*   **값을 바꾸는 법**: 
    1.  `DefaultEngine.ini`에서 `DefaultValueInt` 수정
    2.  게임 실행 중 콘솔(`~` 키)에서 `DDCvar.PawnClass 1` 입력
    3.  블루프린트에서 `ExecuteConsoleCommand`로 변경

이 방식은 코드를 수정하지 않고(ini 수정만으로) 테스트 캐릭터를 쉽게 바꾸기 위해 사용됩니다.
