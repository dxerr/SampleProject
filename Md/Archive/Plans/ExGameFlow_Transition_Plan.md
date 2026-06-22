# 인게임 진입(맵 전환) 흐름의 데이터 드리븐 추상화

현재 로비 위젯의 Blueprint에서 인게임 진입 시 `ExGameFlowSubsystem`의 `TransitionToInGame("L_ExRunnerTest")`를 호출하며 URL을 하드코딩 중입니다.
동적으로 맵을 선택하거나 GameFeature별로 로드할 맵이 달라지는 구조를 대응하기 위해, **Experience(경험) 데이터 에셋 기반의 맵 전환 시스템**으로 아키텍처를 개선합니다.

---

## 아키텍처 설계 방향 (옵션 2: Experience 기반 맵 지정)

UI는 이제 "어떤 맵 이름인지" 알 필요가 없습니다. 대신 **어떤 게임 경험(Experience)을 시작할지**만 선언합니다.

1. **`UExExperienceDefinition` 확장**: 진입할 맵(UWorld) 레퍼런스 필드 추가
2. **`UExGameFlowSubsystem` 확장**: 하드코딩 스트링을 받는 기존 함수 대신, DataAsset ID를 받아 맵 경로를 파싱하는 새 함수(`TransitionToExperience`) 추가
3. **UI(Blueprint) 변경**: Map Name String이 아니라 드롭다운에서 Data Asset 레퍼런스를 골라서 넘기도록 단순화

---

## Proposed Changes

### 1. `UExExperienceDefinition` 확장

`c:\wz\ExFrameWork\Plugins\GameFeatures\ExCore\Source\ExCoreRuntime\Experience\ExExperienceDefinition.h`

기존 UI 레이아웃 지정 기능 외에, 이 경험을 구동할 **메인 맵에 대한 정보**를 추가합니다.

```diff
 UCLASS(BlueprintType, Const)
 class EXCORERUNTIME_API UExExperienceDefinition : public UPrimaryDataAsset
 {
     GENERATED_BODY()

 public:
+    /** 이 게임 경험을 실행할 때 로드할 메인 맵 파일 */
+    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay")
+    TSoftObjectPtr<UWorld> MapToLoad;

     // 이 경험(맵)에서 활성화해야 할 UI 레이아웃 베이스...
     UPROPERTY(EditDefaultsOnly, Category = "UI")
     TSoftClassPtr<UExHUDLayoutWidget> DefaultHUDLayout;
```

---

### 2. `ExGameFlowSubsystem` 인터페이스 변경

`c:\wz\ExFrameWork\Plugins\GameFeatures\ExCore\Source\ExCoreRuntime\Subsystems\ExGameFlowSubsystem.h`

하드코딩 스트링에 의존하던 `TransitionToInGame`을 삭제하거나 방치하는 대신, 에셋 오브젝트를 받는 새로운 **`TransitionToExperience`** 함수로 대체합니다.

```diff
     UFUNCTION(BlueprintCallable, Category = "ExFlow")
     void RequestTravel(const FString& MapURL);

-    /** 
-     * [UI 호출용] 로비에서 인게임으로 서버 이동을 직접 요청합니다.
-     */
-    UFUNCTION(BlueprintCallable, Category = "ExFlow")
-    void TransitionToInGame(const FString& MapURL);
+    /** 
+     * [UI 호출용] 경험 데이터 에셋(Experience Definition) 기반으로 인게임 전환을 요청합니다.
+     * 맵 이름 대신 데이터 에셋을 인자로 받아, 그 안에서 지정된 맵 경로를 찾아 트래블합니다.
+     */
+    UFUNCTION(BlueprintCallable, Category = "ExFlow", meta=(DisplayName="Transition To Experience"))
+    void TransitionToExperience(const UExExperienceDefinition* ExperienceConfig);
```

#### `ExGameFlowSubsystem.cpp` 구현 로직

```cpp
void UExGameFlowSubsystem::TransitionToExperience(const UExExperienceDefinition* ExperienceConfig)
{
    if (!ExperienceConfig)
    {
        UE_LOG(LogTemp, Error, TEXT("[ExGameFlow] TransitionToExperience 실패: 값이 유효하지 않음."));
        return;
    }

    if (ExperienceConfig->MapToLoad.IsNull())
    {
        UE_LOG(LogTemp, Error, TEXT("[ExGameFlow] TransitionToExperience 실패: %s 안에 대상 MapToLoad이 비어있음."), *ExperienceConfig->GetName());
        return;
    }

    // TSoftObjectPtr<UWorld> 경로에서 로드용 URL 스트링 추출 (.umap 경로 형태)
    FString MapURL = ExperienceConfig->MapToLoad.ToSoftObjectPath().GetLongPackageName();

    UE_LOG(LogTemp, Log, TEXT("[ExGameFlow] Experience [%s] 전환 요청. 대상 맵: %s"), *ExperienceConfig->GetName(), *MapURL);

    // 하드코딩이 아님! 데이터 에셋에서 찾은 경로를 쏨
    SetFlowState(ExMatchTags::Flow_InGame);
    OnRequestTravel.Broadcast(MapURL);
}
```

---

### 3. 주인님 수동 작업: 블루프린트(로비 UI 등) 갱신

위 C++ 로직이 적용되어 빌드가 완료되면 다음과 같은 BP 레벨 수정이 발생합니다:

1. **기존 `TransitionToInGame` 노드 에러 발생**: 기존 컴파일 에러 발생 (권장사항: C++ 파일에서 구함수는 삭제)
2. **`TransitionToExperience` 노드로 교체**: 새로 생성된 노드를 연결. 인자로 `ExExperienceDefinition` 객체 참조를 받게 됨.
3. **Data Asset 세팅**: 에디터에서 `DA_RunnerExperience` (또는 비슷한 이름) 에셋을 생성 및 열어서 `MapToLoad` 변수에 `L_ExRunnerTest` 레벨을 지정. 
4. **BP 바인딩**: 로비 BP에서 해당 Data Asset(`DA_RunnerExperience`)을 변수로 갖거나 직접 핀으로 연결.
