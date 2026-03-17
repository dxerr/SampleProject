# 모바일 AsyncLoading 크래시 해결 아키텍처 옵션 정리

- **작성일**: 2026-03-17
- **관련 버그**: `ExportObject.TemplateObject: 'Default__ExInputComponentBase' is not a child of class: 'ExRunnerInputComponent'`
- **원인 요약**: 
  이전 빌드에서는 Core 애셋과 Feature 애셋(`ExRunnerInputComponent`)이 같은 0번 기본 청크(Chunk)로 묶여 성공했을 수 있습니다. 하지만 불필요한 더미 맵, 모바일 기본 UI 등 리다이렉트가 정리되면서 **언리얼 엔진 패키징 청크 계산이 변경**되었고, 그 결과 Feature 모듈(`ExRunnerPlay`)이 후속 청크로 독립 분리되었습니다. 
  게임 구동 시 0번 청크의 Core 폰(`ExSandboxCharacter_Mover`)이 아직 메모리에 마운트되기도 전인 Feature 컴포넌트를 참조하려다 파싱(Memory map)에 실패하고 부모 클래스(`Default__ExInputComponentBase`)로 붕괴(Fallback)하며 크래시가 발생했습니다. (전형적인 GameFeature 하드 레퍼런스 크래시)

---

## 💡 해결 옵션 제안

기존 블루프린트에 작성해둔 이벤트 노드(`On Jump Requested (ExRunnerInput)`)들을 살리면서 구조적 한계를 넘기기 위해 두 가지 방안을 제안합니다.

### ✅ 옵션 1: 컴포넌트를 ExCore 모듈로 이동 시키기 (가장 쉽고 편한 방법)
- **설명**: `ExRunnerInputComponent` C++ 클래스의 소속을 `ExRunnerPlay`에서 `ExCore` 모듈로 옮깁니다.
- **장점**: Core 소속끼리는 패키징 직렬화 규칙 충돌이 일어나지 않습니다. 따라서 현재 블루프린트에 연결해 둔 입력 델리게이트 노드(초록색)를 **단 1개도 수정하지 않고 그대로 유지**할 수 있습니다. 청크(Chunk) 에러도 영구적으로 사라집니다.
- **단점**: Core 모듈 안에 특정 피처(Runner) 전용 컴포넌트가 존재하게 되어, 향후 아키텍처 확장에 있어 약간의 아쉬움이 발생합니다.

### ✅ 옵션 2: Blueprint Interface (BPI) 아키텍처 도입 (언리얼 정석 방식)
- **설명**: 컴포넌트 직접 부착과 델리게이트 노드 연결을 모두 제거하고, `ExSandboxCharacter_Mover` 블루프린트에 **`BPI_RunnerInputReceiver`** (혹은 이와 유사한 인터페이스)를 상속시켜 통신하는 방식으로 변경합니다. 
- **장점**: Core와 Feature 모듈 간의 완벽한 분리가 이루어지며, 프로젝트 덩치가 커져도 매우 안전합니다. 또한 GameFeature가 활성화될 때 런타임에 안전하게 동적 액션(Add Components)으로 주입될 수 있습니다.
- **단점**: 블루프린트에서 기존 컴포넌트를 지우게 되므로, **기존 연결되어 있던 초록색(델리게이트) 이벤트 노드들의 연결을 끊고, 새롭게 추가되는 빨간색(인터페이스) 이벤트 액션 노드로 선을 모두 다시 연결**해야 하는 수작업 노동이 필요합니다.

---

**결정 대기**: 재부팅 후 위의 두 가지 아키텍처 방향 중 하나를 선택하여 지시해주시면, 곧바로 C++ 코드 수정 및 관련 아키텍처 문서의 오기입 내용을 정정하겠습니다.
