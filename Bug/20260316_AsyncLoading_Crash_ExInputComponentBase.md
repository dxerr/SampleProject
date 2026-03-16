# 모바일 AsyncLoading 크래시 오류 해결 (Abstract Class CDO)

## ❌ 오류 증상 (Logcat)
모바일(안드로이드) 빌드 기동 시 다음과 같은 에러 발생과 함께 즉시 튕김 (Crash):
```
Assertion failed: ExportObject.TemplateObject->IsA(LoadClass)
ExportObject.TemplateObject: 'Default__ExInputComponentBase' is not a child of class: 'ExRunnerInputComponent'
```

## 🔍 원인 분석
- `UExInputComponentBase`는 컴포넌트의 추상 기저 클래스(`Abstract`)로 설계되었으나, UCLASS 매크로 내에 인스턴스화를 허용하는 `Blueprintable`과 `meta=(BlueprintSpawnableComponent)` 속성이 그대로 포함되어 있었습니다.
- 자식 클래스인 `UExRunnerInputComponent`를 블루프린트(`ExSandboxCharacter_Mover.uasset`)에 추가하고 패키징하는 과정에서, 언리얼 엔진의 `AsyncLoading`이 `TemplateObject`(CDO 생성 기준 객체)를 자식 클래스가 아닌 부모 기반인 `Default__ExInputComponentBase`로 잘못 생성/참조하는 설계 충돌(직렬화 오류)이 발생했습니다.

## ✅ 해결 방식
- `ExInputComponentBase.h` 파일의 `UCLASS` 매크로에서 파생 자식에게만 필요하고 부모에게는 충돌을 유발하는 속성을 모두 제거했습니다.

**수정 전:**
```cpp
UCLASS(Abstract, Blueprintable, ClassGroup=(ExInput), meta=(BlueprintSpawnableComponent))
class EXCORERUNTIME_API UExInputComponentBase : public UActorComponent
```

**수정 후:**
```cpp
UCLASS(Abstract, ClassGroup=(ExInput))
class EXCORERUNTIME_API UExInputComponentBase : public UActorComponent
```

- 변경 사항 컴파일 후, 엔진 에디터상에서 CDO 직렬화 캐시가 리빌드되므로 블루프린트를 한 번 열어 저장하는 것이 직렬화 최신화에 가장 안전합니다.
