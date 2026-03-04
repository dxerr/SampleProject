# SkyDome 바인딩 및 머티리얼 렌더링 수정

## 버그 현상 파악 (Keywords: SkyDome, Material, TSubclassOf, 노드 연결 해제, 컴파일 에러)
1. **SkyDome 바인딩 문제**: `AExPlayerCameraManager`에서 `TSubclassOf<AActor> TargetSkyDomeClass`를 사용하여 에디터 뷰포트에만 레벨 인스턴스로 존재하는 SkyDome(예: StaticMeshActor)를 찾지 못하는 문제가 발생함. (`UGameplayStatics::GetActorOfClass`가 BP클래스가 아닌 단순 배치 액터를 감지하지 못하는 제약)
2. **머티리얼 파이썬 생성 에러**: `M_ExClearSky_V5` 머티리얼을 코드로 재구축할 때, 핀 이름을 명시적으로 지정("Input", "VectorInput")했으나 언리얼 엔진 내 파이썬 API의 예외 처리에 막혀 각 노드의 선이 하얗게 끊어지고 고아 노드(Orphaned Node)들이 발생하는 문제. 나아가 파라미터 노드에 텍스처를 할당하지 않아 `[SM6] Found NULL, requires Texture2D` 에러가 발생.

## 트러블슈팅 및 해결 과정
1. **바인딩 로직 개선**:
   - `TargetSkyDomeClass` 변수를 삭제하고 **`TargetSkyDomeTag` (FName)** 로 대체.
   - `BeginPlay()` 내부 로직을 `UGameplayStatics::GetAllActorsWithTag`로 변경하여 레벨에 배치된 임의의 액터 중 "SkyDome" 태그를 가진 액터를 유연하게 가져올 수 있도록 개선. (느슨한 결합)
   
2. **머티리얼 스크립트 수정 및 완벽한 그래프 생성**:
   - 기존의 버그가 난 노드들을 지우는 대신 완전히 깨끗한 새 머티리얼인 **`M_ExClearSky_V7`** 을 생성.
   - 노드 연결을 강제할 때, 파인 이름으로의 연결이 실패되는 언리얼 파이썬 API 버그를 우회하기 위해 **빈 문자열 `""`을 사용**하여 연결함으로써 무조건 연결되도록 보장시켜 고아 노드(Orphan Node)가 발생하지 않도록 함.
   - `TilingNoise05` 엔진 기본 텍스처를 스크립트를 통해 명시적으로 검색한 뒤 `TextureSampleParameter2D` 노드에 할당하여 SM6 에러를 근본적으로 제거.

## 결과
- `L_ExRunnerTest` 맵의 SkyDome 액터가 새로운 V7 머티리얼을 사용하도록 정상 교체 및 바인딩 완료.
- C++ 바인딩 코드와 신규 머티리얼 자원을 버전 컨트롤(Git)에 Commit 및 Push 완료.
